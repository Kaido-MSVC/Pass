#include "password.h"
#include <QtTest>
#include <cmath>
#include <random>
#include <stdexcept>
class Tests:public QObject {
 Q_OBJECT
private slots:
 void publicPhrases_data(){
  QTest::addColumn<QString>("input");
  const QStringList weak={"correct horse battery staple","Correct-Horse-Battery-Staple","CORRECT_HORSE_BATTERY_STAPLE!2026",
   "c0rrect h0rse b@ttery st@ple","correcthorsebatterystaple","this is a very secure password",
   "the quick brown fox jumps over the lazy dog","Tr0ub4dor&3","jennifer1985!","December251990"};
  int row=0;for(const auto& value:weak)QTest::newRow(qPrintable(QString::number(row++)))<<value;
 }
 void publicPhrases(){QFETCH(QString,input);QVERIFY2(pass::analyze(input).score<60,qPrintable(QString::number(pass::analyze(input).score)));}
 void phraseControls(){
  for(const auto& input:{QString("correct horse battery staple 7v$K!sP2@zF9mQ4x#D6r"),QString("rivet odyssey glacier thimble cosmos velvet"),QString("7v$K!sP2@zF9mQ4x#D6r")})
   QVERIFY2(pass::analyze(input).score>=80,qPrintable(QString::number(pass::analyze(input).score)));
  // A NUL cannot hide a known prefix/suffix from the C API.
  QCOMPARE(pass::analyze(QString::fromLatin1("random\0password",15)).score,0);
 }
 void estimatorStress(){
  QElapsedTimer elapsed;elapsed.start();
  std::mt19937 fixture(221);std::uniform_int_distribution<int> pick(1,127);
  for(int n=0;n<2000;++n){QString input;for(int i=0;i<1+n%128;++i)input+=QChar(pick(fixture));
   const auto score=pass::analyze(input).score;QVERIFY(score>=0 && score<=100);input.fill(QChar(0));}
  qInfo()<<"Estimator 2000 mixed-length cases ms:"<<elapsed.elapsed();
  QVERIFY(elapsed.elapsed()<30000);
 }

 void generatedRules(){pass::Rules r;QSet<QString> seen;for(int i=0;i<150;++i){auto p=pass::generate(r);QVERIFY(pass::matches(p,r));QVERIFY(!p.contains('0'));QVERIFY(!p.contains('I'));seen.insert(p);}QCOMPARE(seen.size(),150);}
 void invalid(){pass::Rules r;r.length=4;r.minUpper=4;r.minLower=4;QVERIFY(!pass::validate(r).isEmpty());QVERIFY_THROWS_EXCEPTION(std::runtime_error,pass::generate(r));r=pass::Rules{};r.excluded="ABCDEFGHIJKLMNOPQRSTUVWXYZ";QVERIFY(!pass::validate(r).isEmpty());r=pass::Rules{};r.custom="中文";QVERIFY(!pass::validate(r).isEmpty());}
 void customOnly(){pass::Rules r;r.upper=r.lower=r.digits=r.symbols=false;r.custom="aabbcc";r.length=12;QVERIFY(pass::validate(r).isEmpty());for(int i=0;i<30;++i)QVERIFY(pass::matches(pass::generate(r),r));QVERIFY(qAbs(pass::entropy(r)-12*std::log2(3.0))<0.00001);}
 void minima(){pass::Rules r;r.length=8;r.upper=false;r.symbols=false;r.minLower=4;r.minDigits=4;for(int i=0;i<50;++i)QVERIFY(pass::matches(pass::generate(r),r));}
 void entropyCounts(){pass::Rules r;r.upper=false;r.lower=false;r.symbols=false;r.digits=true;r.excluded="23456789";r.excludeAmbiguous=false;r.custom="a";r.minDigits=1;r.length=4;QVERIFY(qAbs(pass::entropy(r)-std::log2(80.0))<0.00001);}
 void strength(){QVERIFY(pass::analyze("password123").score<35);QVERIFY(pass::analyze("aaaaaaaaaaaaaaaa").score<35);QVERIFY(pass::analyze("7v$K!sP2@zF9mQ4x#D6r").score>=80);QCOMPARE(pass::analyze("").score,0);}
 void humanPatterns_data(){
  QTest::addColumn<QString>("input");QTest::addColumn<int>("maximum");
  const QStringList weak={"1qaz@WSX3edc8521313","!QAZ@WSX#EDC","qazwsxedcrfv","zaqXSWcde","159753852","2468013579","97531abcdef","Abc!Abc!Abc!Abc!","xyzxyzxyzxyz","aaaaaaaaaaaaaaaa","P@ssw0rd2026!","ADMIN123456!","welcomeWelcome2026!","passwordqwerty123456","!@#$%^&*()","qwertyuiop","1234567890",QString(128,'a')};
  int index=0;for(const auto& input:weak)QTest::newRow(qPrintable(QString::number(index++)))<<input<<59;
  QTest::newRow("common")<<QString("password123")<<34;
 }
 void humanPatterns(){QFETCH(QString,input);QFETCH(int,maximum);const auto a=pass::analyze(input);QVERIFY2(a.score<=maximum,qPrintable(QString("score=%1").arg(a.score)));QVERIFY(!a.feedback.isEmpty());}
 void randomLookingControls_data(){
  QTest::addColumn<QString>("input");
  const QStringList controls={"7v$K!sP2@zF9mQ4x#D6r","m7#Tq2&Vx9!Lp4$Nz6@R","qwer7v$K!sP2@zF9mQ4x#D6r","password7v$K!sP2@zF9mQ4x#D6r","aaa7v$K!sP2@zF9mQ4x#D6r","1237v$K!sP2@zF9mQ4x#D6r","abcdefgh7v$K!sP2@zF9mQ4x#D6r"};
  int index=0;for(const auto& input:controls)QTest::newRow(qPrintable(QString::number(index++)))<<input;
 }
 void randomLookingControls(){QFETCH(QString,input);QVERIFY(pass::analyze(input).score>=80);}
 void randomCorpus(){
  // A reproducible synthetic uniform corpus measures broad false-positive risk;
  // this PRNG is only a test fixture, never a password generator.
  const QString alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()-_=+[]{};:,.?/";
  std::mt19937 fixture(20261015);std::uniform_int_distribution<int> pick(0,int(alphabet.size())-1);
  int belowStrong=0,belowVeryStrong=0;
  for(int i=0;i<1000;++i){QString input;for(int k=0;k<20;++k)input+=alphabet[pick(fixture)];const int score=pass::analyze(input).score;belowStrong+=score<60;belowVeryStrong+=score<80;input.fill(QChar(0));}
  qInfo()<<"Uniform fixture: below strong"<<belowStrong<<"below very strong"<<belowVeryStrong<<"of 1000";
  QVERIFY(belowStrong<=5);QVERIFY(belowVeryStrong<=20);
  // Exercise real outputs of the unchanged BCrypt provider; never print or persist them.
  {
   pass::Rules r;int low=0;
   for(int i=0;i<50;++i){auto input=pass::generate(r);low+=pass::analyze(input).score<60;input.fill(QChar(0));}
   QVERIFY(low<=2);
  }
 }
 void reportedRegression(){const auto result=pass::analyze("1qaz@WSX3edc8521313");qInfo()<<"Reported regression score:"<<result.score;QVERIFY(result.score<60);}
 void analysisBoundaries(){
  QCOMPARE(pass::analyze("").score,0);
  for(const auto& input:{QString("a"),QString(128,'x'),QString::fromUtf8("中文かな🔑"),QString::fromLatin1("a\0b",3)}){const auto a=pass::analyze(input);QVERIFY(a.score>=0 && a.score<=100);}
  QCOMPARE(pass::analyze(QString(129,'x')).label,QStringLiteral("尚未評估"));
  QCOMPARE(pass::analyze("1qaz2wsx3edc").score,pass::analyze("!QAZ@WSX#EDC").score);
  QElapsedTimer timer;timer.start();for(int i=0;i<20;++i)pass::analyze(QString(128,'a'));QVERIFY(timer.elapsed()<3000);
 }
 void cancellation(){pass::Rules r;QVERIFY_THROWS_EXCEPTION(std::runtime_error,pass::generate(r,[]{return true;}));}
};
QTEST_GUILESS_MAIN(Tests)
#include "password_test.moc"
