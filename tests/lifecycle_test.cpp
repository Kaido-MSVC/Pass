#include <windows.h>
#include <openssl/evp.h>
#include <openssl/provider.h>
#include <openssl/core_names.h>
#include <openssl/err.h>
#include <QtTest>
#include <atomic>
#include <condition_variable>
#include <mutex>
namespace inject {
std::mutex mutex;
std::condition_variable cv;
bool release=false;
bool stallPermanently=false;
std::atomic_bool entered{false}, zeroOK{true};
std::atomic_int wipes{0};
int random(EVP_RAND_CTX*, unsigned char* buffer, size_t size, unsigned int, int, const unsigned char*, size_t) {
    entered=true;
    if(stallPermanently)Sleep(INFINITE);
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock,[]{return release;});
    std::fill(buffer,buffer+size,0);
    return 1;
}
PVOID wipe(PVOID buffer,SIZE_T size) {
    auto result=::SecureZeroMemory(buffer,size);
    for(SIZE_T i=0;i<size;++i)if(static_cast<unsigned char*>(buffer)[i])zeroOK=false;
    ++wipes;return result;
}
}
#define EVP_RAND_generate inject::random
#undef SecureZeroMemory
#define SecureZeroMemory inject::wipe
#include "../src/secure_random.cpp"
#include "../src/password.cpp"
#include "../src/passphrase.cpp"
#include "../src/generation.h"
#include "../src/generation.cpp"
#undef EVP_RAND_generate
#undef SecureZeroMemory
class LifecycleTests:public QObject {
 Q_OBJECT
private slots:
 void secureStorage(){
  inject::wipes=0;inject::zeroOK=true;
  {SecretText value;value.reserve(16);value.append(QStringLiteral("secret"));
   QVERIFY_THROWS_EXCEPTION(std::length_error,value.append(QString(100,'x')));
   QCOMPARE(value.text(),QStringLiteral("secret"));value.clear();QVERIFY(value.empty());
   value.append(QStringLiteral("second"));}
  QCOMPARE(inject::wipes.load(),2);QVERIFY(inject::zeroOK.load());
 }
 void stalledOpenSSLCancellation(){
  struct Release {~Release(){ {std::lock_guard<std::mutex> lock(inject::mutex);inject::release=true;}inject::cv.notify_all();while(generationBusy.load())QTest::qWait(1);}} release;
  pass::Rules r;r.upper=r.lower=r.digits=r.symbols=false;r.custom="ab";
  auto job=startGeneration(r,1);QVERIFY(job);QTRY_VERIFY(inject::entered.load());
  QElapsedTimer timer;timer.start();job->abandon();QVERIFY(timer.elapsed()<500);
  QVERIFY(!job->done.load());QVERIFY(!startGeneration(r,1));
  {std::lock_guard<std::mutex> lock(inject::mutex);inject::release=true;}inject::cv.notify_all();
  QTRY_VERIFY(job->done.load());QVERIFY(job->secret.empty());QVERIFY(inject::zeroOK.load());
 }
 void processExitWithStalledOpenSSL(){
  QProcess child;child.start(QCoreApplication::applicationFilePath(),{QStringLiteral("--blocked-exit")});
  QVERIFY(child.waitForStarted(5000));QVERIFY(child.waitForFinished(5000));
  QCOMPARE(child.exitStatus(),QProcess::NormalExit);QCOMPARE(child.exitCode(),0);
 }
 void stalledWordOpenSSLCancellation(){
  {std::lock_guard<std::mutex> lock(inject::mutex);inject::release=false;}inject::entered=false;inject::zeroOK=true;
  struct Release {~Release(){ {std::lock_guard<std::mutex> lock(inject::mutex);inject::release=true;}inject::cv.notify_all();while(generationBusy.load())QTest::qWait(1);}} release;
  auto job=startGeneration(pass::PassphraseRules{},100);QVERIFY(job);QTRY_VERIFY(inject::entered.load());
  QElapsedTimer timer;timer.start();job->abandon();QVERIFY(timer.elapsed()<500);
  QVERIFY(!job->done.load());QVERIFY(!startGeneration(pass::Rules{},1));QVERIFY(!startGeneration(pass::PassphraseRules{},1));
  {std::lock_guard<std::mutex> lock(inject::mutex);inject::release=true;}inject::cv.notify_all();
  QTRY_VERIFY(job->done.load());QVERIFY(job->secret.empty());QVERIFY(inject::zeroOK.load());
 }
 void processExitWithStalledWordOpenSSL(){
  QProcess child;child.start(QCoreApplication::applicationFilePath(),{QStringLiteral("--blocked-word-exit")});
  QVERIFY(child.waitForStarted(5000));QVERIFY(child.waitForFinished(5000));
  QCOMPARE(child.exitStatus(),QProcess::NormalExit);QCOMPARE(child.exitCode(),0);
 }
};
int main(int argc,char** argv){
 QCoreApplication app(argc,argv);
 if(app.arguments().contains(QStringLiteral("--blocked-exit")) || app.arguments().contains(QStringLiteral("--blocked-word-exit"))){
  inject::stallPermanently=true;auto job=app.arguments().contains(QStringLiteral("--blocked-word-exit")) ? startGeneration(pass::PassphraseRules{},1) : startGeneration(pass::Rules{},1);
  QElapsedTimer timer;timer.start();while(!inject::entered.load() && timer.elapsed()<3000)QThread::msleep(1);
  if(!inject::entered.load())return 2;job->abandon();return 0;
 }
 LifecycleTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "lifecycle_test.moc"

