#include <QtWidgets>
#include <QtTest>
#include <QResource>
#undef Q_INIT_RESOURCE
#define Q_INIT_RESOURCE(name) do {} while(false)
#include "../src/wordlist.cpp"
#define PASS_UI_TEST
#include "../src/main.cpp"
class MissingWordlist : public QObject {
    Q_OBJECT
    QTemporaryDir settingsDir;
private slots:
    void initTestCase() { QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settingsDir.path());QCoreApplication::setOrganizationName("PassTests");QCoreApplication::setApplicationName("MissingList"); }
    void onlyWordModeDisabled() {
        QVERIFY(!pass::wordlist().available()); QVERIFY(!pass::wordlist().error.isEmpty());
        Window w;auto tabs=w.findChild<QTabWidget*>("modeTabs");auto generate=w.findChild<QPushButton*>("primary");
        QVERIFY(generate->isEnabled());generate->click();QTRY_VERIFY(!w.findChild<QPlainTextEdit*>()->toPlainText().isEmpty());
        tabs->setCurrentIndex(1);QVERIFY(!generate->isEnabled());QVERIFY(w.findChild<QPlainTextEdit*>()->toPlainText().isEmpty());
        tabs->setCurrentIndex(0);QVERIFY(generate->isEnabled());generate->click();QTRY_VERIFY(!w.findChild<QPlainTextEdit*>()->toPlainText().isEmpty());
    }
};
QTEST_MAIN(MissingWordlist)
#include "missing_wordlist_test.moc"
