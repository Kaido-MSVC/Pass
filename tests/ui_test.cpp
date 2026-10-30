#define PASS_UI_TEST
#include "../src/main.cpp"
#include <QtTest>
#include <condition_variable>
#include <iostream>
namespace {
std::mutex blockMutex;
std::condition_variable blockCv;
bool released=false;
std::atomic_int calls{0};
QString blockingGenerator(const pass::Rules&, const std::function<bool()>&) {
    ++calls;
    std::unique_lock<std::mutex> lock(blockMutex);
    blockCv.wait(lock,[]{return released;});
    return QStringLiteral("Abc!2345");
}
struct BlockScope {
    BlockScope() {std::lock_guard<std::mutex> lock(blockMutex);released=false;calls=0;}
    void release() { {std::lock_guard<std::mutex> lock(blockMutex);released=true;} blockCv.notify_all(); }
    ~BlockScope() {release();while(generationBusy.load())QTest::qWait(1);}
};
QPushButton* button(Window& w,const QString& text) {for(auto b:w.findChildren<QPushButton*>())if(b->text()==text)return b;return nullptr;}
QString partialThenBlock(const pass::Rules& r,const std::function<bool()>& c) {
    if(calls++==0)return QStringLiteral("Abc!2345");
    return blockingGenerator(r,c);
}
QString partialThenFail(const pass::Rules&,const std::function<bool()>&) {
    if(calls++==0)return QStringLiteral("Abc!2345");
    throw std::runtime_error("injected failure");
}
QString blockingWordGenerator(const pass::PassphraseRules&,const std::function<bool()>& c) { return blockingGenerator(pass::Rules{},c); }
QString longestWordGenerator(const pass::PassphraseRules& r,const std::function<bool()>&) { return QStringList(r.words,QStringLiteral("abdominal")).join(r.separator); }
QString partialWordThenFail(const pass::PassphraseRules&,const std::function<bool()>&) {
    if(calls++==0)return QStringLiteral("abacus-abacus-abacus-abacus-abacus-abacus");
    throw std::runtime_error("injected word failure");
}
QString partialWordThenBlock(const pass::PassphraseRules& r,const std::function<bool()>& c) {
    if(calls++==0)return QStringLiteral("abacus-abacus-abacus-abacus-abacus-abacus");
    return blockingWordGenerator(r,c);
}
}
class UiTests:public QObject {
 Q_OBJECT
 QTemporaryDir settingsDir;
private slots:
 void clipboardEmptyFailureRetries(){
  struct Restore {~Restore(){clearClipboardContents=&::EmptyClipboard;}} restore;
  Window w;w.findChild<QPushButton*>("primary")->click();QTRY_VERIFY(!w.findChild<QPlainTextEdit*>()->toPlainText().isEmpty());
  button(w,QStringLiteral("複製全部"))->click();QVERIFY(!QApplication::clipboard()->text().isEmpty());
  static int attempts;attempts=0;
  clearClipboardContents=+[]()->BOOL {return attempts++==0 ? FALSE : ::EmptyClipboard();};
  auto timer=w.findChild<QTimer*>("clipboardTimer");timer->start(1);
  QTRY_COMPARE(attempts,1);QVERIFY(timer->isActive());
  QTRY_COMPARE(attempts,2);QVERIFY(!timer->isActive());QVERIFY(QApplication::clipboard()->text().isEmpty());
 }
 void clipboardLockedExpiry_data(){
  QTest::addColumn<int>("mode");
  QTest::newRow("real-30s-expiry-recovers")<<0;
  QTest::newRow("later-different-copy-survives")<<1;
  QTest::newRow("later-identical-copy-survives")<<2;
  QTest::newRow("persistent-lock-stops")<<3;
  QTest::newRow("disable-cancels-retry")<<4;
  QTest::newRow("destroy-cancels-retry")<<5;
 }
 void clipboardLockedExpiry(){
  QFETCH(int,mode);
  auto w=std::make_unique<Window>();w->findChild<QPushButton*>("primary")->click();
  QTRY_VERIFY(!w->findChild<QPlainTextEdit*>()->toPlainText().isEmpty());
  button(*w,QStringLiteral("複製全部"))->click();
  QString original=QApplication::clipboard()->text();CandidateWiper wipe(original);QVERIFY(!original.isEmpty());
  auto timer=w->findChild<QTimer*>("clipboardTimer");QVERIFY(timer->isActive());
  QProcess locker;locker.start(QCoreApplication::applicationFilePath(),{QStringLiteral("--lock-clipboard")});
  QVERIFY(locker.waitForStarted(5000));QVERIFY(locker.waitForReadyRead(5000));QCOMPARE(locker.readAllStandardOutput().trimmed(),QByteArray("locked"));
  if(mode==0){QCOMPARE(timer->interval(),30000);QTest::qWait(29000);QCOMPARE(timer->interval(),30000);QVERIFY(timer->isActive());QTest::qWait(1500);}
  else {timer->start(1);QTest::qWait(250);}
  QVERIFY(timer->isActive());QCOMPARE(timer->interval(),100);
  if(mode==3){QTest::qWait(5500);QVERIFY(!timer->isActive());}
  if(mode==4){for(auto cb:w->findChildren<QCheckBox*>())if(cb->text()==QStringLiteral("複製後 30 秒清除剪貼簿"))cb->setChecked(false);QVERIFY(!timer->isActive());}
  if(mode==5)w.reset();
  locker.write("release\n");QVERIFY(locker.waitForBytesWritten(5000));QVERIFY(locker.waitForFinished(5000));QCOMPARE(locker.exitCode(),0);
  if(mode==1 || mode==2){
   const QString later=mode==1?QStringLiteral("later external contents"):original;
   QApplication::clipboard()->setText(later);QTest::qWait(350);
   QCOMPARE(QApplication::clipboard()->text(),later);QVERIFY(!timer->isActive());
  } else if(mode==0){QTRY_VERIFY_WITH_TIMEOUT(QApplication::clipboard()->text().isEmpty(),2000);QVERIFY(!timer->isActive());}
  else {QTest::qWait(350);QCOMPARE(QApplication::clipboard()->text(),original);}
  QApplication::clipboard()->clear();
 }
 void publicPhraseLanguages(){
  Window w;auto field=w.findChildren<QLineEdit*>().last();auto strength=w.findChild<QProgressBar*>();
  for(int language=0;language<3;++language){w.findChild<QComboBox*>("uiLanguage")->setCurrentIndex(language);
   field->setText("correct horse battery staple");QVERIFY(strength->value()<35);field->clear();}
 }
 void initTestCase(){QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settingsDir.path());QCoreApplication::setOrganizationName("PassTests");QCoreApplication::setApplicationName("Ui");applyAppearance(*qApp);}
 void init(){
  // Previous window destruction is intentionally non-blocking. Finish its
  // abandoned worker before a new fixture clicks a freshly created button.
  QTRY_VERIFY_WITH_TIMEOUT(!generationBusy.load(),12000);
  QSettings().clear();QSettings().sync();pass::uiLanguage=0;
 }
 void patternStrengthLanguages(){
  Window w;
  auto check=w.findChildren<QLineEdit*>().last();auto strength=w.findChild<QProgressBar*>();
  for(int language=0;language<3;++language){
   auto lang=w.findChild<QComboBox*>("uiLanguage");lang->setCurrentIndex(language);
   check->setText("1qaz@WSX3edc8521313");QVERIFY(strength->value()<60);
   QCOMPARE(strength->value(),pass::analyze(check->text()).score);
   const auto analysis=pass::analyze(check->text());
   for(const auto& feedback:analysis.feedback){const auto translated=pass::uiText(feedback);QVERIFY(!translated.isEmpty());if(pass::uiLanguage)QVERIFY(translated!=feedback);
    bool shown=false;for(auto label:w.findChildren<QLabel*>())shown|=label->text().contains(translated);QVERIFY(shown);}
   check->setText("7v$K!sP2@zF9mQ4x#D6r");QVERIFY(strength->value()>=80);
   check->clear();QCOMPARE(strength->value(),0);
  }
 }
 void languagesAndPreferenceSafety(){
  QSettings s;s.setValue("custom","secret marker");s.setValue("excluded","private");s.setValue("password","never retain");s.sync();
  {Window w;auto lang=w.findChild<QComboBox*>("uiLanguage");QVERIFY(lang);
   auto fields=w.findChildren<QLineEdit*>();for(auto field:fields)QVERIFY(!field->text().contains("secret marker"));
   for(auto code:{"en","ja","zh-TW"}){lang->setCurrentIndex(lang->findData(code));QCOMPARE(QSettings().value("ui/language").toString(),QString::fromLatin1(code));
    if(QString(code)=="en")QCOMPARE(w.findChild<QPushButton*>("primary")->text(),QStringLiteral("Generate"));
    if(QString(code)=="ja")QCOMPARE(w.findChild<QPushButton*>("primary")->text(),QStringLiteral("生成"));}
   lang->setCurrentIndex(lang->findData("en"));
   w.findChild<QSpinBox*>("characterLength")->setValue(4);w.findChild<QSpinBox*>("minimum0")->setValue(8);
   bool translated=false;for(auto label:w.findChildren<QLabel*>())translated|=label->text().contains("sum of minimum");QVERIFY(translated);
  }
  s.sync();QVERIFY(!s.contains("custom"));QVERIFY(!s.contains("excluded"));QVERIFY(!s.contains("password"));
  {Window restored;QCOMPARE(restored.findChild<QComboBox*>("uiLanguage")->currentData().toString(),QStringLiteral("en"));}
 }
 void languageSwitchDuringWork(){BlockScope block;Window w(blockingGenerator);w.findChild<QPushButton*>("primary")->click();QTRY_COMPARE(calls.load(),1);
  w.findChild<QComboBox*>("uiLanguage")->setCurrentIndex(2);QVERIFY(generationBusy.load());block.release();QTRY_VERIFY(!w.findChild<QPlainTextEdit*>()->toPlainText().isEmpty());}
 void savedLanguageSurvivesProcessRestart(){for(auto code:{"zh-TW","ja","en"}){
  {Window w;auto combo=w.findChild<QComboBox*>("uiLanguage");combo->setCurrentIndex(combo->findData(code));}
  QProcess restarted;restarted.start(QCoreApplication::applicationFilePath(),{QStringLiteral("--restore-language"),settingsDir.path(),QString::fromLatin1(code)});
  QVERIFY(restarted.waitForStarted(5000));QVERIFY(restarted.waitForFinished(10000));QCOMPARE(restarted.exitStatus(),QProcess::NormalExit);QCOMPARE(restarted.exitCode(),0);
 }}
 void stockQtMenusTranslate(){Window w;auto language=w.findChild<QComboBox*>("uiLanguage");auto field=w.findChildren<QLineEdit*>().first();
  language->setCurrentIndex(language->findData("en"));std::unique_ptr<QMenu> english(field->createStandardContextMenu());const QString original=english->actions().first()->text();
  for(auto code:{"ja","zh-TW"}){language->setCurrentIndex(language->findData(code));std::unique_ptr<QMenu> translated(field->createStandardContextMenu());QVERIFY(translated->actions().first()->text()!=original);}
 }
 void algorithmUiSwitch1000Times(){Window w;auto combo=w.findChild<QComboBox*>("randomAlgorithm");for(int i=0;i<1000;++i){combo->setCurrentIndex(i%4);QCOMPARE(combo->currentIndex(),i%4);QVERIFY(w.findChild<QPlainTextEdit*>()->toPlainText().isEmpty());}QVERIFY(!generationBusy.load());}
 void laterIdenticalClipboardCopySurvives(){Window w;w.findChild<QPushButton*>("primary")->click();QTRY_VERIFY(!w.findChild<QPlainTextEdit*>()->toPlainText().isEmpty());
  button(w,QStringLiteral("複製全部"))->click();QString value=QApplication::clipboard()->text();QVERIFY(!value.isEmpty());
  const auto before=GetClipboardSequenceNumber();QApplication::clipboard()->setText(value);QVERIFY(GetClipboardSequenceNumber()!=before);
  w.findChild<QTimer*>("clipboardTimer")->start(1);QTest::qWait(30);QCOMPARE(QApplication::clipboard()->text(),value);wipeText(value);
 }
 void clipboardCountdown(){Window w;w.findChild<QPushButton*>("primary")->click();QTRY_VERIFY(!w.findChild<QPlainTextEdit*>()->toPlainText().isEmpty());button(w,QStringLiteral("複製全部"))->click();
  QTest::qWait(250);bool countdown=false;for(auto label:w.findChildren<QLabel*>())countdown|=label->text().contains(QStringLiteral("秒後安全清除"));QVERIFY(countdown);
  QApplication::clipboard()->setText("external");QTRY_VERIFY(!w.findChild<QTimer*>("clipboardCountdown")->isActive());
 }
 void algorithmSelectionAndPersistence(){
  for(int choice=0;choice<4;++choice){
   {Window w;auto combo=w.findChild<QComboBox*>("randomAlgorithm");QVERIFY(combo);combo->setCurrentIndex((choice+1)%4);combo->setCurrentIndex(choice);
    QCOMPARE(QSettings().value("random/algorithm").toString(),combo->currentData().toString());
    w.findChild<QPushButton*>("primary")->click();QTRY_VERIFY(!w.findChild<QPlainTextEdit*>()->toPlainText().isEmpty());
    combo->setCurrentIndex((choice+1)%4);QVERIFY(w.findChild<QPlainTextEdit*>()->toPlainText().isEmpty());combo->setCurrentIndex(choice);}
   Window restored;QCOMPARE(restored.findChild<QComboBox*>("randomAlgorithm")->currentIndex(),choice);
  }
 }
 void savedAlgorithmSurvivesProcessRestart(){
  for(int choice=0;choice<4;++choice){
   {Window w;auto combo=w.findChild<QComboBox*>("randomAlgorithm");combo->setCurrentIndex((choice+1)%4);combo->setCurrentIndex(choice);}
   QProcess restarted;
   restarted.start(QCoreApplication::applicationFilePath(),{QStringLiteral("--restore-algorithm"),settingsDir.path(),QString::number(choice)});
   QVERIFY(restarted.waitForStarted(5000));
   // Keep the parent Qt event loop responsive during child GUI/clipboard startup.
   QTRY_COMPARE_WITH_TIMEOUT(restarted.state(),QProcess::NotRunning,10000);
   QCOMPARE(restarted.exitStatus(),QProcess::NormalExit);QCOMPARE(restarted.exitCode(),0);
  }
 }
 void invalidAlgorithmFailsClosed(){
  QSettings().setValue("random/algorithm","unknown-drbg");Window w;
  QVERIFY(!w.findChild<QPushButton*>("primary")->isEnabled());
  auto combo=w.findChild<QComboBox*>("randomAlgorithm");QCOMPARE(combo->currentData().toString(),QStringLiteral("unknown-drbg"));
  combo->setCurrentIndex(1);QVERIFY(w.findChild<QPushButton*>("primary")->isEnabled());
 }
 void switchingAlgorithmCancelsActiveWork(){
  BlockScope block;Window w(blockingGenerator);w.findChild<QPushButton*>("primary")->click();QTRY_COMPARE(calls.load(),1);
  w.findChild<QComboBox*>("randomAlgorithm")->setCurrentIndex(2);
  block.release();QTRY_VERIFY(!generationBusy.load());QTest::qWait(60);QVERIFY(w.findChild<QPlainTextEdit*>()->toPlainText().isEmpty());
 }
 void nextJobUsesSelectedAlgorithm(){
  static std::atomic_int seen{-1};
  auto capture=+[](const pass::Rules& r,const std::function<bool()>& c){seen=static_cast<int>(r.algorithm);return pass::generate(r,c);};
  Window w(capture);
  for(int choice=0;choice<4;++choice){w.findChild<QComboBox*>("randomAlgorithm")->setCurrentIndex(choice);seen=-1;
   w.findChild<QPushButton*>("primary")->click();QTRY_COMPARE(seen.load(),choice);QTRY_VERIFY(!w.findChild<QPlainTextEdit*>()->toPlainText().isEmpty());}
 }
 void blockedCancellationAndLateResult(){
  BlockScope block;Window w(blockingGenerator);auto out=w.findChild<QPlainTextEdit*>();
  w.findChild<QPushButton*>("primary")->click();QTRY_COMPARE(calls.load(),1);
  QElapsedTimer elapsed;elapsed.start();button(w,QStringLiteral("取消產生"))->click();QVERIFY(elapsed.elapsed()<500);
  QVERIFY(!startGeneration(pass::Rules{},1,blockingGenerator));QCOMPARE(calls.load(),1);
  block.release();QTRY_VERIFY(!generationBusy.load());QTest::qWait(60);QVERIFY(out->toPlainText().isEmpty());
  w.findChild<QPushButton*>("primary")->click();QTRY_VERIFY(!out->toPlainText().isEmpty());
 }
 void blockedWindowDestruction(){
  BlockScope block;auto w=std::make_unique<Window>(blockingGenerator);w->findChild<QPushButton*>("primary")->click();QTRY_COMPARE(calls.load(),1);
  QElapsedTimer elapsed;elapsed.start();w.reset();QVERIFY(elapsed.elapsed()<500);QVERIFY(generationBusy.load());
 }
 void blockedTimeout(){
  BlockScope block;Window w(blockingGenerator);w.findChild<QPushButton*>("primary")->click();QTRY_COMPARE(calls.load(),1);
  auto cancel=button(w,QStringLiteral("取消產生"));QVERIFY(cancel->isEnabled());
  QTRY_VERIFY_WITH_TIMEOUT(!cancel->isEnabled(),12000);QVERIFY(generationBusy.load());
  QVERIFY(w.findChild<QPlainTextEdit*>()->toPlainText().isEmpty());
  bool timeout=false;for(auto label:w.findChildren<QLabel*>())timeout|=label->text().contains(QStringLiteral("逾時"));QVERIFY(timeout);
 }
 void partialBatchClearedOnCancel(){
  BlockScope block;auto job=startGeneration(pass::Rules{},2,partialThenBlock);QVERIFY(job);QTRY_VERIFY(calls.load()>=3);
  {std::lock_guard<std::mutex> lock(job->mutex);QCOMPARE(job->secret.text(),QStringLiteral("Abc!2345"));}
  job->abandon();{std::lock_guard<std::mutex> lock(job->mutex);QVERIFY(job->secret.empty());}
  block.release();QTRY_VERIFY(job->done.load());QVERIFY(job->secret.empty());
 }
 void partialBatchClearedOnFailure(){
  calls=0;auto job=startGeneration(pass::Rules{},2,partialThenFail);QVERIFY(job);
  QTRY_VERIFY(job->done.load());QVERIFY(job->secret.empty());QCOMPARE(job->error,QStringLiteral("injected failure"));
 }
 void generateRevealCopyClear(){
  Window w;w.show();auto generate=w.findChild<QPushButton*>("primary");auto output=w.findChild<QPlainTextEdit*>();QVERIFY(generate);QVERIFY(output);
  w.findChild<QSpinBox*>("characterBatch")->setValue(3);generate->click();
  QTRY_VERIFY_WITH_TIMEOUT(!output->toPlainText().isEmpty(),10000);
  QVERIFY(!output->property("secret").isValid());QVERIFY(!output->isUndoRedoEnabled());QVERIFY(w.windowTitle().contains(QStringLiteral("v" PASS_VERSION_STRING)));
  QCheckBox* reveal=nullptr;for(auto box:w.findChildren<QCheckBox*>())if(box->text()==QStringLiteral("顯示密碼"))reveal=box;QVERIFY(reveal);
  const QString hidden=output->toPlainText();reveal->setChecked(true);
  const QString secrets=output->toPlainText();QCOMPARE(secrets.split('\n').size(),3);QVERIFY(hidden!=secrets);
  reveal->setChecked(false);QCOMPARE(output->toPlainText(),hidden);reveal->setChecked(true);QCOMPARE(output->toPlainText(),secrets);
  QPushButton *copy=nullptr,*clear=nullptr;for(auto button:w.findChildren<QPushButton*>()){if(button->text()==QStringLiteral("複製全部"))copy=button;if(button->text()==QStringLiteral("清除結果"))clear=button;}QVERIFY(copy);QVERIFY(clear);
  copy->click();QCOMPARE(QApplication::clipboard()->text(),secrets);auto timer=w.findChild<QTimer*>("clipboardTimer");QVERIFY(timer);
  QApplication::clipboard()->setText("other application content");timer->start(1);QTest::qWait(30);QCOMPARE(QApplication::clipboard()->text(),QString("other application content"));
  copy->click();timer->start(1);QTRY_VERIFY(QApplication::clipboard()->text().isEmpty());
  copy->click();QCOMPARE(QApplication::clipboard()->text(),secrets);
  clear->click();QVERIFY(QApplication::clipboard()->text().isEmpty());QVERIFY(output->toPlainText().isEmpty());QVERIFY(output->property("secret").toString().isEmpty());
  output->undo();QVERIFY(output->toPlainText().isEmpty());
  w.findChild<QSpinBox*>("characterLength")->setValue(4);w.findChild<QSpinBox*>("minimum0")->setValue(8);QVERIFY(!generate->isEnabled());
 }
 void wordBatchLongestClipboard(){
  Window w(static_cast<Generator>(pass::generate),longestWordGenerator);w.show();
  w.findChild<QTabWidget*>("modeTabs")->setCurrentIndex(1);
  auto count=w.findChild<QSpinBox*>("wordCount");auto batch=w.findChild<QSpinBox*>("wordBatch");QCOMPARE(count->value(),6);QCOMPARE(batch->value(),1);
  QCOMPARE(w.findChild<QComboBox*>("separator")->currentData().toString(),QStringLiteral("-"));
  count->setValue(12);batch->setValue(100);w.findChild<QPushButton*>("primary")->click();
  auto output=w.findChild<QPlainTextEdit*>();QTRY_VERIFY_WITH_TIMEOUT(!output->toPlainText().isEmpty(),10000);
  for(auto box:w.findChildren<QCheckBox*>())if(box->text()==QStringLiteral("顯示密碼"))box->setChecked(true);
  QString text=output->toPlainText();QCOMPARE(text.size(),11999);QCOMPARE(text.split('\n').size(),100);
  for(const auto& line:text.split('\n'))QCOMPARE(line.size(),119);
  auto check=w.findChildren<QLineEdit*>().last();QCOMPARE(check->text().size(),119);
  QVERIFY(w.findChild<QLabel*>("resultLength")->text().contains("119"));
  button(w,QStringLiteral("複製全部"))->click();QCOMPARE(QApplication::clipboard()->text(),text);
  auto timer=w.findChild<QTimer*>("clipboardTimer");QCOMPARE(timer->interval(),30000);
  timer->start(1);QTRY_VERIFY(QApplication::clipboard()->text().isEmpty());
  button(w,QStringLiteral("複製全部"))->click();QApplication::clipboard()->setText("new external data");
  timer->start(1);QTest::qWait(30);QCOMPARE(QApplication::clipboard()->text(),QStringLiteral("new external data"));
  button(w,QStringLiteral("複製全部"))->click();w.findChild<QTabWidget*>("modeTabs")->setCurrentIndex(0);
  QVERIFY(output->toPlainText().isEmpty());QVERIFY(check->text().isEmpty());QVERIFY(QApplication::clipboard()->text().isEmpty());
  for(auto box:w.findChildren<QCheckBox*>())if(box->text()==QStringLiteral("顯示密碼"))QVERIFY(!box->isChecked());
  wipeText(text);
 }
 void wordLifecycle_data(){QTest::addColumn<QString>("action");for(const auto& action:{"cancel","clear","switch","close","timeout"})QTest::newRow(action)<<QString::fromLatin1(action);}
 void wordLifecycle(){
  QFETCH(QString,action);BlockScope block;auto w=std::make_unique<Window>(static_cast<Generator>(pass::generate),blockingWordGenerator);
  w->findChild<QTabWidget*>("modeTabs")->setCurrentIndex(1);w->findChild<QPushButton*>("primary")->click();QTRY_COMPARE(calls.load(),1);
  QElapsedTimer elapsed;elapsed.start();
  if(action=="close")w.reset();
  else if(action=="cancel")button(*w,QStringLiteral("取消產生"))->click();
  else if(action=="clear")button(*w,QStringLiteral("清除結果"))->click();
  else if(action=="switch")w->findChild<QTabWidget*>("modeTabs")->setCurrentIndex(0);
  else {QTRY_VERIFY_WITH_TIMEOUT(!button(*w,QStringLiteral("取消產生"))->isEnabled(),12000);}
  if(action!="timeout")QVERIFY(elapsed.elapsed()<500);
  QVERIFY(generationBusy.load());QVERIFY(!startGeneration(pass::Rules{},1));QVERIFY(!startGeneration(pass::PassphraseRules{},1));
  if(w)QVERIFY(w->findChild<QPlainTextEdit*>()->toPlainText().isEmpty());
  block.release();QTRY_VERIFY(!generationBusy.load());QTest::qWait(60);
  if(w){QVERIFY(w->findChild<QPlainTextEdit*>()->toPlainText().isEmpty());QTRY_VERIFY(w->findChild<QPushButton*>("primary")->isEnabled());}
 }
 void characterToWordSwitchRejectsLateResult(){
  BlockScope block;Window w(blockingGenerator);auto tabs=w.findChild<QTabWidget*>("modeTabs");
  w.findChild<QPushButton*>("primary")->click();QTRY_COMPARE(calls.load(),1);
  QElapsedTimer elapsed;elapsed.start();tabs->setCurrentIndex(1);QVERIFY(elapsed.elapsed()<500);
  QVERIFY(!w.findChild<QPushButton*>("primary")->isEnabled());block.release();QTRY_VERIFY(!generationBusy.load());QTest::qWait(60);
  QVERIFY(w.findChild<QPlainTextEdit*>()->toPlainText().isEmpty());QTRY_VERIFY(w.findChild<QPushButton*>("primary")->isEnabled());
  w.findChild<QPushButton*>("primary")->click();QTRY_VERIFY(!w.findChild<QPlainTextEdit*>()->toPlainText().isEmpty());
 }
 void partialWordBatchClearedOnFailure(){
  calls=0;auto job=startGeneration(pass::PassphraseRules{},2,partialWordThenFail);QVERIFY(job);QTRY_VERIFY(job->done.load());
  QVERIFY(job->secret.empty());QCOMPARE(job->error,QStringLiteral("injected word failure"));
 }
 void partialWordBatchClearedOnCancel(){
  BlockScope block;auto job=startGeneration(pass::PassphraseRules{},2,partialWordThenBlock);QVERIFY(job);QTRY_VERIFY(calls.load()>=3);
  {std::lock_guard<std::mutex> lock(job->mutex);QVERIFY(!job->secret.empty());}
  job->abandon();{std::lock_guard<std::mutex> lock(job->mutex);QVERIFY(job->secret.empty());}
  block.release();QTRY_VERIFY(job->done.load());QVERIFY(job->secret.empty());
 }
 void compatibleSettingsAndSeparatePages(){
  QSettings settings;settings.setValue("length",32);settings.setValue("batch",3);settings.setValue("custom","~");settings.setValue("autoClear",false);
  {Window w;QCOMPARE(w.findChild<QSpinBox*>("characterLength")->value(),32);QCOMPARE(w.findChild<QSpinBox*>("characterBatch")->value(),3);
   w.findChild<QTabWidget*>("modeTabs")->setCurrentIndex(1);w.findChild<QSpinBox*>("wordCount")->setValue(8);w.findChild<QSpinBox*>("wordBatch")->setValue(4);w.findChild<QComboBox*>("separator")->setCurrentIndex(2);
   w.findChild<QTabWidget*>("modeTabs")->setCurrentIndex(0);QCOMPARE(w.findChild<QSpinBox*>("characterLength")->value(),32);QCOMPARE(w.findChild<QSpinBox*>("characterBatch")->value(),3);}
  settings.sync();QCOMPARE(settings.value("length").toInt(),32);QCOMPARE(settings.value("passphrase/words").toInt(),8);QCOMPARE(settings.value("passphrase/separator").toString(),QStringLiteral(" "));
  settings.setValue("length",24);settings.sync(); // v1.01 writes only its existing flat keys.
  {Window w;QCOMPARE(w.findChild<QSpinBox*>("characterLength")->value(),24);QCOMPARE(w.findChild<QSpinBox*>("wordCount")->value(),8);QCOMPARE(w.findChild<QSpinBox*>("wordBatch")->value(),4);QCOMPARE(w.findChild<QComboBox*>("separator")->currentIndex(),2);QCOMPARE(w.findChild<QTabWidget*>("modeTabs")->currentIndex(),0);}
  const QSet<QString> expected={"ui/appearance","ui/language","random/algorithm","length","batch","ambiguous","autoClear","enabled0","enabled1","enabled2","enabled3","minimum0","minimum1","minimum2","minimum3","passphrase/words","passphrase/batch","passphrase/separator"};
  settings.sync();for(const auto& key:settings.allKeys())QVERIFY(expected.contains(key));
 }
 void menuAndThemePersistence(){
  for(auto theme:{"system","light","dark"}){
   {Window w;auto menu=w.findChild<QMenu*>("appearanceMenu");auto combo=w.findChild<QComboBox*>("uiAppearance");
    QCOMPARE(w.menuBar()->actions().size(),4);QCOMPARE(menu->actions().size(),3);
    menu->actions()[combo->findData(theme)]->trigger();
    QCOMPARE(QSettings().value("ui/appearance").toString(),QString::fromLatin1(theme));
    int checked=0;for(auto action:menu->actions())checked+=action->isChecked();QCOMPARE(checked,1);
    const bool dark=QString::fromLatin1(theme)=="dark" || (QString::fromLatin1(theme)=="system" && qApp->styleHints()->colorScheme()==Qt::ColorScheme::Dark);
    QVERIFY((qApp->palette().color(QPalette::Window).lightness()<128)==dark);
    QVERIFY(w.aboutText().contains(QStringLiteral("Pass-SSL v" PASS_VERSION_STRING)));QVERIFY(w.aboutText().contains(QStringLiteral(PASS_AUTHOR)));QVERIFY(w.aboutText().contains(QString::fromLatin1(OpenSSL_version(OPENSSL_VERSION))));
    auto algorithms=w.findChild<QMenu*>("algorithmMenu");QCOMPARE(algorithms->actions().size(),4);
    algorithms->actions()[3]->trigger();QCOMPARE(w.findChild<QComboBox*>("randomAlgorithm")->currentData().toString(),QStringLiteral("HMAC-DRBG-SHA512"));
    QVERIFY(w.findChild<QLabel*>("algorithmInfo")->text().contains("SHA-512"));
   }
   {Window w;QCOMPARE(w.findChild<QComboBox*>("uiAppearance")->currentData().toString(),QString::fromLatin1(theme));}
   QProcess child;child.start(QCoreApplication::applicationFilePath(),{"--restore-theme",settingsDir.path(),QString::fromLatin1(theme)});
   QVERIFY(child.waitForStarted(5000));QVERIFY(child.waitForFinished(10000));QCOMPARE(child.exitCode(),0);
  }
  QSettings().setValue("ui/appearance","broken");Window w;QCOMPARE(w.findChild<QComboBox*>("uiAppearance")->currentData().toString(),QStringLiteral("system"));
 }
 void aboutDialog(){
  Window w;w.show();bool checked=false;
  QTimer::singleShot(100,&w,[&]{auto dialog=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
   if(dialog){checked=dialog->text().contains(w.aboutText());dialog->grab().save(QCoreApplication::applicationDirPath()+"/about-v" PASS_VERSION_STRING ".png");dialog->accept();}
  });
  w.findChild<QAction*>("aboutAction")->trigger();QVERIFY(checked);
 }
 void nativeSpinButtonsAndTabFeedback(){
  Window w;w.show();
  auto tabs=w.findChild<QTabWidget*>("modeTabs");auto bar=tabs->tabBar();
  for(auto theme:{"light","dark"}){
   auto appearance=w.findChild<QComboBox*>("uiAppearance");
   w.findChild<QMenu*>("appearanceMenu")->actions()[appearance->findData(theme)]->trigger();
   for(int mode=0;mode<2;++mode){
    tabs->setCurrentIndex(mode);QTest::qWait(40);
    for(auto spin:tabs->currentWidget()->findChildren<QSpinBox*>()){
     QVERIFY(spin->isVisible());QVERIFY(spin->isEnabled());
     const int original=spin->value(), middle=(spin->minimum()+spin->maximum())/2;
     spin->setValue(middle);
     QStyleOptionSpinBox option;option.initFrom(spin);option.rect=spin->rect();
     option.frame=true;option.buttonSymbols=spin->buttonSymbols();option.stepEnabled=QAbstractSpinBox::StepUpEnabled|QAbstractSpinBox::StepDownEnabled;
     const QRect up=spin->style()->subControlRect(QStyle::CC_SpinBox,&option,QStyle::SC_SpinBoxUp,spin);
     const QRect down=spin->style()->subControlRect(QStyle::CC_SpinBox,&option,QStyle::SC_SpinBoxDown,spin);
     QVERIFY(!up.isEmpty());QVERIFY(!down.isEmpty());QVERIFY(!up.intersects(down));
     QTest::mouseMove(w.windowHandle(),QPoint(1,1));QTest::qWait(10);
     const QImage image=spin->grab().toImage().scaled(spin->size());
     for(const auto& rect:{up,down}){
      int low=255,high=0;
      const QRect glyph=QRect(rect.center()-QPoint(3,3),QSize(7,7)).intersected(image.rect());
      for(int y=glyph.top();y<=glyph.bottom();++y)for(int x=glyph.left();x<=glyph.right();++x){
       const int gray=qGray(image.pixel(x,y));low=qMin(low,gray);high=qMax(high,gray);
      }
      QVERIFY2(high-low>=60,qPrintable(QString("Missing/low-contrast arrow: %1 %2").arg(theme,spin->objectName())));
     }
     QTest::mouseClick(spin,Qt::LeftButton,Qt::NoModifier,up.center());QCOMPARE(spin->value(),middle+1);
     QTest::mouseClick(spin,Qt::LeftButton,Qt::NoModifier,down.center());QCOMPARE(spin->value(),middle);
     spin->setFocus();QTest::keyClick(spin,Qt::Key_Up);QCOMPARE(spin->value(),middle+1);
     QTest::keyClick(spin,Qt::Key_Down);QCOMPARE(spin->value(),middle);
     spin->selectAll();QTest::keyClicks(spin,QString::number(middle+2));QTest::keyClick(spin,Qt::Key_Return);QCOMPARE(spin->value(),middle+2);
     spin->setValue(spin->maximum());QTest::mouseClick(spin,Qt::LeftButton,Qt::NoModifier,up.center());QCOMPARE(spin->value(),spin->maximum());
     spin->setValue(spin->minimum());QTest::mouseClick(spin,Qt::LeftButton,Qt::NoModifier,down.center());QCOMPARE(spin->value(),spin->minimum());
     spin->setValue(middle);spin->setEnabled(false);QTest::mouseClick(spin,Qt::LeftButton,Qt::NoModifier,up.center());QCOMPARE(spin->value(),middle);
     spin->setEnabled(true);spin->setValue(original);
    }
   }
   tabs->setCurrentIndex(0);QTest::mouseMove(w.windowHandle(),QPoint(1,1));QTest::qWait(30);
   const auto normal=bar->grab().toImage();const QRect tabRect=bar->tabRect(1);
   QTest::mouseMove(w.windowHandle(),bar->mapTo(&w,tabRect.center()));QTest::qWait(30);const auto hovered=bar->grab().toImage();
   QVERIFY(normal!=hovered);QVERIFY(bar->grab().save(QCoreApplication::applicationDirPath()+QString("/tabs-%1-hover.png").arg(theme)));
   // Qt selects tabs on mouse press; the filled segment gives immediate feedback.
   QTest::mousePress(bar,Qt::LeftButton,Qt::NoModifier,tabRect.center());QCOMPARE(tabs->currentIndex(),1);
   const auto pressed=bar->grab().toImage();QVERIFY(pressed!=hovered);
   QVERIFY(bar->grab().save(QCoreApplication::applicationDirPath()+QString("/tabs-%1-pressed.png").arg(theme)));
   QTest::mouseRelease(bar,Qt::LeftButton,Qt::NoModifier,tabRect.center());QCOMPARE(tabs->currentIndex(),1);
   QTest::mouseMove(w.windowHandle(),QPoint(1,1));QTest::qWait(20);
   QVERIFY(bar->grab().save(QCoreApplication::applicationDirPath()+QString("/tabs-%1-selected.png").arg(theme)));
   bar->setFocus();QTest::keyClick(bar,Qt::Key_Left);QCOMPARE(tabs->currentIndex(),0);
  }
 }
 void finalPolishFeedback(){
  Window w;w.show();
  auto tabs=w.findChild<QTabWidget*>("modeTabs");
  for(auto theme:{"light","dark"}){
   auto appearance=w.findChild<QComboBox*>("uiAppearance");
   w.findChild<QMenu*>("appearanceMenu")->actions()[appearance->findData(theme)]->trigger();
   tabs->setCurrentIndex(0);QTest::qWait(40);
   auto card=w.findChild<QGroupBox*>("rulesCard");QVERIFY(card);
   QCOMPARE(card->grab().toImage().pixelColor(20,30),QColor(QString(theme)=="dark"?"#252e39":"#edf1f5"));
   auto field=w.findChild<QLineEdit*>();QVERIFY(field);
   for(auto spin:tabs->currentWidget()->findChildren<QSpinBox*>())
    QCOMPARE(spin->palette().color(QPalette::Base),QColor(QString(theme)=="dark"?"#29323e":"#ffffff"));
   const QStringList labels={QStringLiteral("產生密碼"),QStringLiteral("取消產生"),QStringLiteral("複製全部"),QStringLiteral("清除結果")};
   for(int i=0;i<labels.size();++i){
    auto b=button(w,labels[i]);QVERIFY(b);const bool wasEnabled=b->isEnabled();b->setEnabled(true);
    QSignalSpy clicks(b,&QPushButton::clicked);
    // Exercise the real QSS renderer without depending on the OS cursor or active window.
    const QRect geometry=b->geometry();
    auto capture=[&](const QString& state){
     QImage shot(b->size(),QImage::Format_ARGB32_Premultiplied);shot.fill(Qt::transparent);
     QStyleOptionButton option;option.initFrom(b);option.rect=b->rect();option.text=b->text();
     option.state&=~(QStyle::State_MouseOver|QStyle::State_Sunken|QStyle::State_HasFocus);
     option.state|=QStyle::State_Raised;
     if(state=="hover")option.state|=QStyle::State_MouseOver;
     if(b->isDown()){option.state&=~QStyle::State_Raised;option.state|=QStyle::State_Sunken;}
     QPainter painter(&shot);b->style()->drawControl(QStyle::CE_PushButton,&option,&painter,b);painter.end();
     shot.save(QCoreApplication::applicationDirPath()+QString("/button-%1-%2-%3.png").arg(theme).arg(i).arg(state));
     return shot.pixelColor(12,12);
    };
    const QColor normal=capture("normal");const QColor hover=capture("hover");
    QVERIFY2(normal!=hover,qPrintable(QString("Hover feedback: %1 button %2").arg(theme).arg(i)));
    QTest::mousePress(b,Qt::LeftButton,Qt::NoModifier,b->rect().center());QVERIFY(b->isDown());const QColor pressed=capture("pressed");
    QVERIFY(hover!=pressed);QVERIFY(normal!=pressed);QCOMPARE(b->geometry(),geometry);
    QTest::mouseRelease(b,Qt::LeftButton,Qt::NoModifier,QPoint(-5,-5));QVERIFY(!b->isDown());QCOMPARE(clicks.count(),0);
    b->setEnabled(false);const QColor disabled=capture("disabled");QVERIFY(disabled!=normal);QVERIFY(disabled!=pressed);
    QTest::mouseClick(b,Qt::LeftButton);QCOMPARE(clicks.count(),0);QCOMPARE(b->geometry(),geometry);
    b->setEnabled(wasEnabled);
   }
   for(int mode=0;mode<2;++mode){tabs->setCurrentIndex(mode);QTest::mouseMove(w.windowHandle(),QPoint(1,1));QTest::qWait(30);
    QVERIFY(w.grab().save(QCoreApplication::applicationDirPath()+QString("/polish-%1-%2.png").arg(theme).arg(mode)));
   }
  }
 }
 void themeLanguageMatrix(){
  Window w;w.show();
  for(auto code:{"zh-TW","ja","en"})for(auto theme:{"system","light","dark"}){
   auto lang=w.findChild<QComboBox*>("uiLanguage");w.findChild<QMenu*>("languageMenu")->actions()[lang->findData(code)]->trigger();
   auto appearance=w.findChild<QComboBox*>("uiAppearance");w.findChild<QMenu*>("appearanceMenu")->actions()[appearance->findData(theme)]->trigger();
   for(int mode=0;mode<2;++mode){w.findChild<QTabWidget*>("modeTabs")->setCurrentIndex(mode);QTest::qWait(80);
    auto generate=w.findChild<QPushButton*>("primary");QVERIFY(w.rect().contains(generate->mapTo(&w,generate->rect().bottomRight())));
    QVERIFY(w.grab().save(QCoreApplication::applicationDirPath()+QString("/matrix-%1-%2-%3.png").arg(code,theme).arg(mode)));
   }
   QVERIFY(!w.findChild<QMenu*>("helpMenu")->title().isEmpty());
  }
 }
 void visualPreviews(){
  Window w;w.show();QTest::qWait(100);
  const QRect available=QGuiApplication::primaryScreen()->availableGeometry();
  qInfo()<<"Window"<<w.size()<<"available"<<available.size()<<"minimum"<<w.minimumSizeHint();
  QVERIFY(w.grab().save(QCoreApplication::applicationDirPath()+"/ui-character.png"));
  QVERIFY(w.width()<=available.width());QVERIFY(w.height()<=available.height());
  auto generate=w.findChild<QPushButton*>("primary");QVERIFY(w.rect().contains(generate->mapTo(&w,generate->rect().bottomRight())));
  QVERIFY(w.grab().save(QCoreApplication::applicationDirPath()+"/ui-character.png"));
  w.findChild<QTabWidget*>("modeTabs")->setCurrentIndex(1);QTest::qWait(100);
  QVERIFY(w.rect().contains(generate->mapTo(&w,generate->rect().bottomRight())));
  QVERIFY(w.grab().save(QCoreApplication::applicationDirPath()+"/ui-words.png"));
  for(auto code:{"zh-TW","ja","en"}){
   auto language=w.findChild<QComboBox*>("uiLanguage");language->setCurrentIndex(language->findData(code));QTest::qWait(100);
   for(int mode=0;mode<2;++mode){w.findChild<QTabWidget*>("modeTabs")->setCurrentIndex(mode);QTest::qWait(50);
    QVERIFY(w.rect().contains(generate->mapTo(&w,generate->rect().bottomRight())));
    QVERIFY(w.grab().save(QCoreApplication::applicationDirPath()+QString("/ui-%1-%2.png").arg(code).arg(mode)));}
  }
 }
 void corruptSettingsUseDefaults(){
  QSettings s;s.setValue("length",-99);s.setValue("batch","invalid");s.setValue("minimum0",99);s.setValue("passphrase/words",100);s.setValue("passphrase/batch",0);s.setValue("passphrase/separator","");
  Window w;QCOMPARE(w.findChild<QSpinBox*>("characterLength")->value(),20);QCOMPARE(w.findChild<QSpinBox*>("characterBatch")->value(),1);QCOMPARE(w.findChild<QSpinBox*>("minimum0")->value(),1);
  QCOMPARE(w.findChild<QSpinBox*>("wordCount")->value(),6);QCOMPARE(w.findChild<QSpinBox*>("wordBatch")->value(),1);QCOMPARE(w.findChild<QComboBox*>("separator")->currentIndex(),0);
  QVERIFY(w.findChild<QPushButton*>("primary")->isEnabled());
 }
};
int main(int argc,char** argv){
 QApplication app(argc,argv);
 if(app.arguments().contains("--lock-clipboard")){
  if(!OpenClipboard(nullptr))return 10;
  std::cout<<"locked"<<std::endl;std::string command;std::getline(std::cin,command);
  return CloseClipboard()?0:11;
 }
 if(app.arguments().size()==4 && (app.arguments()[1]=="--restore-algorithm" || app.arguments()[1]=="--restore-language" || app.arguments()[1]=="--restore-theme")){
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,app.arguments()[2]);
  QCoreApplication::setOrganizationName("PassTests");QCoreApplication::setApplicationName("Ui");
  Window restored;
  if(app.arguments()[1]=="--restore-theme")return restored.findChild<QComboBox*>("uiAppearance")->currentData().toString()==app.arguments()[3]?0:4;
  if(app.arguments()[1]=="--restore-language")return restored.findChild<QComboBox*>("uiLanguage")->currentData().toString()==app.arguments()[3]?0:3;
  return restored.findChild<QComboBox*>("randomAlgorithm")->currentIndex()==app.arguments()[3].toInt()?0:2;
 }
 UiTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "ui_test.moc"
