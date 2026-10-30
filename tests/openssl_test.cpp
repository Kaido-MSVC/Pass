#include <windows.h>
#include <openssl/evp.h>
#include <openssl/provider.h>
#include <openssl/core_names.h>
#include <openssl/err.h>
#include <QtTest>
#include <cstring>
#include <set>
#include <thread>
#include "secure_random.h"
namespace fault {
int stage=0, fetches=0, generations=0, uninstantiated=0, reseeds=0, live=0;
bool zeroOK=true, bytesZero=true;
const char* selected=nullptr;
QString actualDigest;
OSSL_LIB_CTX* libraryContext(){if(stage==12)return nullptr;return OSSL_LIB_CTX_new();}
OSSL_PROVIDER* providerLoad(OSSL_LIB_CTX* c,const char* name){if(stage==13)return nullptr;return OSSL_PROVIDER_load(c,name);}
EVP_RAND_CTX* context(EVP_RAND* r,EVP_RAND_CTX* parent){
    if((stage==9 && !parent)||(stage==10 && parent))return nullptr;
    auto c=EVP_RAND_CTX_new(r,parent);if(c)++live;return c;
}
void freeContext(EVP_RAND_CTX* c){if(c)--live;EVP_RAND_CTX_free(c);}
int reseed(EVP_RAND_CTX* c,int pr,const unsigned char* ent,size_t n,const unsigned char* add,size_t an){
    ++reseeds;if(stage==8)return 0;return EVP_RAND_reseed(c,pr,ent,n,add,an);
}
int state(EVP_RAND_CTX* c){if(stage==11 || (stage==14 && generations>0) || (stage==15 && reseeds>0))return EVP_RAND_STATE_ERROR;return EVP_RAND_get_state(c);}
EVP_RAND* fetch(OSSL_LIB_CTX* c,const char* name,const char* prop) {
    ++fetches;
    if(std::strcmp(name,"SEED-SRC")) selected=name;
    if((stage==1 && !std::strcmp(name,"SEED-SRC")) || (stage==2 && std::strcmp(name,"SEED-SRC"))) return nullptr;
    return EVP_RAND_fetch(c,name,prop);
}
int instantiate(EVP_RAND_CTX* c,unsigned int strength,int pr,const unsigned char* p,size_t n,const OSSL_PARAM* params) {
    bool seed=EVP_RAND_is_a(EVP_RAND_CTX_get0_rand(c),"SEED-SRC");
    if((stage==3 && seed)||(stage==4 && !seed))return 0;
    int result=EVP_RAND_instantiate(c,strength,pr,p,n,params);
    if(result && !seed && !EVP_RAND_is_a(EVP_RAND_CTX_get0_rand(c),"CTR-DRBG")){
        char digest[80]{};OSSL_PARAM query[]={OSSL_PARAM_construct_utf8_string(OSSL_DRBG_PARAM_DIGEST,digest,sizeof(digest)),OSSL_PARAM_construct_end()};
        if(EVP_RAND_CTX_get_params(c,query)==1)actualDigest=QString::fromLatin1(digest);
    }
    return result;
}
int generate(EVP_RAND_CTX* c,unsigned char* out,size_t n,unsigned int strength,int pr,const unsigned char* a,size_t an) {
    ++generations;
    if(stage==5 || (stage==6 && generations==2)){std::memset(out,0x77,n);return 0;}
    if(stage==7){std::memset(out,255,n);return 1;}
    return EVP_RAND_generate(c,out,n,strength,pr,a,an);
}
int uninstantiate(EVP_RAND_CTX* c) {
    int r=EVP_RAND_uninstantiate(c);
    if(!EVP_RAND_is_a(EVP_RAND_CTX_get0_rand(c),"SEED-SRC")) {++uninstantiated;zeroOK &= EVP_RAND_verify_zeroization(c)==1;}
    return r;
}
PVOID wipe(PVOID p,SIZE_T n) {auto result=::SecureZeroMemory(p,n);for(SIZE_T i=0;i<n;++i)bytesZero &= static_cast<unsigned char*>(p)[i]==0;return result;}
}
#define EVP_RAND_fetch fault::fetch
#define EVP_RAND_instantiate fault::instantiate
#define EVP_RAND_generate fault::generate
#define EVP_RAND_uninstantiate fault::uninstantiate
#define EVP_RAND_reseed fault::reseed
#define EVP_RAND_CTX_new fault::context
#define EVP_RAND_CTX_free fault::freeContext
#define EVP_RAND_get_state fault::state
#define OSSL_LIB_CTX_new fault::libraryContext
#define OSSL_PROVIDER_load fault::providerLoad
#undef SecureZeroMemory
#define SecureZeroMemory fault::wipe
#include "../src/secure_random.cpp"
#undef EVP_RAND_fetch
#undef EVP_RAND_instantiate
#undef EVP_RAND_generate
#undef EVP_RAND_uninstantiate
#undef EVP_RAND_reseed
#undef EVP_RAND_CTX_new
#undef EVP_RAND_CTX_free
#undef EVP_RAND_get_state
#undef OSSL_LIB_CTX_new
#undef OSSL_PROVIDER_load
#undef SecureZeroMemory
class OpenSslTests:public QObject {
 Q_OBJECT
 void algorithms(){QTest::addColumn<int>("algorithm");for(int a=0;a<4;++a)QTest::newRow(pass::algorithmId(static_cast<pass::RandomAlgorithm>(a)))<<a;}
private slots:
 void init(){fault::stage=fault::fetches=fault::generations=fault::uninstantiated=fault::reseeds=0;fault::zeroOK=fault::bytesZero=true;fault::selected=nullptr;fault::actualDigest.clear();}
 void cleanup(){QCOMPARE(fault::live,0);}
 void reseedSuccess_data(){algorithms();}
 void reseedSuccess(){
  QFETCH(int,algorithm);{pass::Random rng(static_cast<pass::RandomAlgorithm>(algorithm));rng.byte();rng.reseed();rng.byte();QCOMPARE(fault::reseeds,1);QCOMPARE(fault::generations,2);}
  QVERIFY(fault::zeroOK);QVERIFY(fault::bytesZero);
 }
 void libraryFailures_data(){QTest::addColumn<int>("algorithm");QTest::addColumn<int>("stage");for(int a=0;a<4;++a)for(int s:{12,13})QTest::newRow(qPrintable(QString("%1-stage-%2").arg(a).arg(s)))<<a<<s;}
 void libraryFailures(){QFETCH(int,algorithm);QFETCH(int,stage);fault::stage=stage;bool first=false,second=false;
  std::thread worker([&]{pass::Random rng(static_cast<pass::RandomAlgorithm>(algorithm));try{rng.byte();}catch(const std::runtime_error&){first=true;}try{rng.byte();}catch(const std::runtime_error&){second=true;}});worker.join();
  QVERIFY(first);QVERIFY(second);QCOMPARE(fault::fetches,0);QCOMPARE(fault::live,0);QVERIFY(fault::bytesZero);
 }
 void addedFailures_data(){QTest::addColumn<int>("algorithm");QTest::addColumn<int>("stage");for(int a=0;a<4;++a)for(int s:{8,9,10,11,14,15})QTest::newRow(qPrintable(QString("%1-stage-%2").arg(a).arg(s)))<<a<<s;}
 void addedFailures(){
  QFETCH(int,algorithm);QFETCH(int,stage);pass::Random rng(static_cast<pass::RandomAlgorithm>(algorithm));
  if(stage==8 || stage==15)rng.byte();fault::stage=stage;
  if(stage==8 || stage==15)QVERIFY_THROWS_EXCEPTION(std::runtime_error,rng.reseed());else QVERIFY_THROWS_EXCEPTION(std::runtime_error,rng.byte());
  int fetches=fault::fetches,generations=fault::generations,reseeds=fault::reseeds;
  QVERIFY_THROWS_EXCEPTION(std::runtime_error,rng.byte());QVERIFY_THROWS_EXCEPTION(std::runtime_error,rng.reseed());
  QCOMPARE(fault::fetches,fetches);QCOMPARE(fault::generations,generations);QCOMPARE(fault::reseeds,reseeds);QCOMPARE(fault::live,0);QVERIFY(fault::bytesZero);QVERIFY(fault::zeroOK);
 }
 void automaticReseed_data(){algorithms();}
 void automaticReseed(){QFETCH(int,algorithm);pass::Random rng(static_cast<pass::RandomAlgorithm>(algorithm));for(int i=0;i<4096*256+1;++i)rng.byte();QCOMPARE(fault::reseeds,1);}
 void boundary_data(){algorithms();}
 void boundary(){QFETCH(int,algorithm);pass::Random rng(static_cast<pass::RandomAlgorithm>(algorithm));
  for(int n:{0,-1,257})QVERIFY_THROWS_EXCEPTION(std::runtime_error,rng.below(n));
  for(int n:{0,-1,65537})QVERIFY_THROWS_EXCEPTION(std::runtime_error,rng.below16(n));QCOMPARE(fault::fetches,0);
  QCOMPARE(rng.below(1),0);QVERIFY(rng.below(256)<256);QCOMPARE(rng.below16(1),0);QVERIFY(rng.below16(65536)<65536);
 }
 void switching1000Times(){
  for(int i=0;i<1000;++i){ {pass::Random rng(static_cast<pass::RandomAlgorithm>(i%4));rng.byte();if(i%10==0)rng.reseed();}QCOMPARE(fault::live,0);}
  QCOMPARE(fault::uninstantiated,1000);QVERIFY(fault::zeroOK);QVERIFY(fault::bytesZero);
 }
 void version(){QCOMPARE(QString::fromLatin1(OpenSSL_version(OPENSSL_VERSION)),QStringLiteral("OpenSSL 4.0.3 29 Sep 2026"));}
 void realProvider_data(){algorithms();}
 void realProvider(){
  QFETCH(int,algorithm);auto a=static_cast<pass::RandomAlgorithm>(algorithm);
  {pass::Random first(a),second(a);QByteArray x,y;for(int i=0;i<512;++i){x+=char(first.byte());y+=char(second.byte());}
   if(a==pass::RandomAlgorithm::Hmac512)QVERIFY(fault::actualDigest=="SHA2-512" || fault::actualDigest=="SHA512");
   else if(a!=pass::RandomAlgorithm::Ctr)QVERIFY(fault::actualDigest=="SHA2-256" || fault::actualDigest=="SHA256");
   QVERIFY(x!=y);QCOMPARE(QString::fromLatin1(fault::selected),QString::fromLatin1(pass::algorithmName(a)));QCOMPARE(fault::fetches,4);x.fill(0);y.fill(0);}
  QCOMPARE(fault::uninstantiated,2);QVERIFY(fault::zeroOK);QVERIFY(fault::bytesZero);
 }
 void failClosed_data(){QTest::addColumn<int>("algorithm");QTest::addColumn<int>("stage");for(int a=0;a<4;++a)for(int s=1;s<=6;++s)QTest::newRow(qPrintable(QString("%1-stage-%2").arg(pass::algorithmId(static_cast<pass::RandomAlgorithm>(a))).arg(s)))<<a<<s;}
 void failClosed(){
  QFETCH(int,algorithm);QFETCH(int,stage);fault::stage=stage;
  {pass::Random rng(static_cast<pass::RandomAlgorithm>(algorithm));
   if(stage==6)for(int i=0;i<256;++i)rng.byte();
   QVERIFY_THROWS_EXCEPTION(std::runtime_error,rng.byte());int fetched=fault::fetches,generated=fault::generations;
   QVERIFY_THROWS_EXCEPTION(std::runtime_error,rng.byte());QCOMPARE(fault::fetches,fetched);QCOMPARE(fault::generations,generated);
   QVERIFY(fault::fetches<=2);QVERIFY(fault::bytesZero);}
  QVERIFY(fault::zeroOK);QVERIFY(fault::bytesZero);
 }
 void rejectionAndCancellation_data(){algorithms();}
 void rejectionAndCancellation(){
  QFETCH(int,algorithm);auto a=static_cast<pass::RandomAlgorithm>(algorithm);fault::stage=7;
  {pass::Random rng(a);QVERIFY_THROWS_EXCEPTION(std::runtime_error,rng.below(3));QCOMPARE(fault::generations,4);}
  fault::generations=0;
  {pass::Random rng(a);QVERIFY_THROWS_EXCEPTION(std::runtime_error,rng.below16(7776));QCOMPARE(fault::generations,8);}
  fault::generations=0;int polls=0;
  {pass::Random rng(a);QVERIFY_THROWS_EXCEPTION(std::runtime_error,rng.below(3,[&]{return ++polls==4;}));QCOMPARE(polls,4);QCOMPARE(fault::generations,1);}
  fault::fetches=0;
  {pass::Random rng(a);QVERIFY_THROWS_EXCEPTION(std::runtime_error,rng.below16(7776,[]{return true;}));QCOMPARE(fault::fetches,0);}
  QVERIFY(fault::zeroOK);QVERIFY(fault::bytesZero);
 }
 void unknownSelection(){pass::Random rng(static_cast<pass::RandomAlgorithm>(99));QVERIFY_THROWS_EXCEPTION(std::runtime_error,rng.byte());QCOMPARE(fault::fetches,0);}
};
QTEST_GUILESS_MAIN(OpenSslTests)
#include "openssl_test.moc"
