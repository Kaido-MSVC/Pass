#define PASS_UI_TEST
#include "../src/main.cpp"
#include <QtTest>
#include <condition_variable>
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
 void initTestCase(){QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settingsDir.path());QCoreApplication::setOrganizationName("PassTests");QCoreApplication::setApplicationName("Ui");applyAppearance(*qApp);}
 void init(){QSettings().clear();}
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
  QVERIFY(!output->property("secret").isValid());QVERIFY(!output->isUndoRedoEnabled());QVERIFY(w.windowTitle().contains("v2.0"));
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
  const QSet<QString> expected={"length","batch","custom","excluded","ambiguous","autoClear","enabled0","enabled1","enabled2","enabled3","minimum0","minimum1","minimum2","minimum3","passphrase/words","passphrase/batch","passphrase/separator"};
  settings.sync();for(const auto& key:settings.allKeys())QVERIFY(expected.contains(key));
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
 }
 void corruptSettingsUseDefaults(){
  QSettings s;s.setValue("length",-99);s.setValue("batch","invalid");s.setValue("minimum0",99);s.setValue("passphrase/words",100);s.setValue("passphrase/batch",0);s.setValue("passphrase/separator","");
  Window w;QCOMPARE(w.findChild<QSpinBox*>("characterLength")->value(),20);QCOMPARE(w.findChild<QSpinBox*>("characterBatch")->value(),1);QCOMPARE(w.findChild<QSpinBox*>("minimum0")->value(),1);
  QCOMPARE(w.findChild<QSpinBox*>("wordCount")->value(),6);QCOMPARE(w.findChild<QSpinBox*>("wordBatch")->value(),1);QCOMPARE(w.findChild<QComboBox*>("separator")->currentIndex(),0);
  QVERIFY(w.findChild<QPushButton*>("primary")->isEnabled());
 }
};
QTEST_MAIN(UiTests)
#include "ui_test.moc"
