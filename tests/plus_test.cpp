#define PASS_UI_TEST
#include "../src/main.cpp"
#include <QtTest>
#include <cmath>

namespace {
const QString symbols="!@#$%^&*()-_=+[]{};:,.?/~<>|";
QString clipboardFixture(const pass::Rules&,const std::function<bool()>&) { return symbols; }
QPushButton* namedButton(Window& w,const QString& caption) {
    for(auto b:w.findChildren<QPushButton*>())if(b->text()==caption)return b;
    return nullptr;
}
}
class PlusTests:public QObject {
    Q_OBJECT
    QTemporaryDir settings;
private slots:
    void initTestCase(){
        QVERIFY(settings.isValid());QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
        QCoreApplication::setOrganizationName("PassPlusTests");QCoreApplication::setApplicationName("Plus");
    }
    void init(){QSettings().clear();}
    void asciiPolicy(){
        QCOMPARE(symbols.size(),28);
        pass::Rules r;r.upper=r.lower=r.digits=false;r.excludeAmbiguous=false;r.minSymbols=0;r.length=4;
        for(int c=0;c<=127;++c){const QString text(4,QChar(c));QCOMPARE(pass::matches(text,r),symbols.contains(QChar(c)));}
        QVERIFY(qAbs(pass::entropy(r)-4*std::log2(28.0))<1e-9);
        r.custom=symbols+symbols;QVERIFY(qAbs(pass::entropy(r)-4*std::log2(28.0))<1e-9);
        r.excludeAmbiguous=true;QVERIFY(!pass::matches("||||",r));
        QVERIFY(qAbs(pass::entropy(r)-4*std::log2(27.0))<1e-9);
        r.excludeAmbiguous=false;r.excluded="~<>|";
        QVERIFY(qAbs(pass::entropy(r)-4*std::log2(24.0))<1e-9);
        r.excluded.clear();r.minSymbols=8;r.length=8;
        for(int i=0;i<50;++i)QVERIFY(pass::matches(pass::generate(r),r));
    }
    void customCompatibility(){
        pass::Rules r;r.upper=r.lower=r.digits=r.symbols=false;r.excludeAmbiguous=false;r.length=4;
        for(int c=33;c<=126;++c)r.custom+=QChar(c);
        for(int c=33;c<=126;++c)QVERIFY(pass::matches(QString(4,QChar(c)),r));
        QVERIFY(qAbs(pass::entropy(r)-4*std::log2(94.0))<1e-9);
    }
    void clipboardRoundTrip(){
        Window w(clipboardFixture);w.findChild<QSpinBox*>("characterLength")->setValue(symbols.size());w.findChild<QSpinBox*>("characterBatch")->setValue(2);
        w.findChild<QPushButton*>("primary")->click();
        auto out=w.findChild<QPlainTextEdit*>();QTRY_VERIFY(!out->toPlainText().isEmpty());
        // Copy from masked results; no HTML interpretation of < > or punctuation.
        QVERIFY(out->toPlainText()!=symbols+"\n"+symbols);
        auto copy=namedButton(w,QStringLiteral("複製全部"));QVERIFY(copy);copy->click();
        QCOMPARE(QApplication::clipboard()->text(),symbols+"\n"+symbols);
        namedButton(w,QStringLiteral("清除結果"))->click();QVERIFY(QApplication::clipboard()->text().isEmpty());
    }
    void appearanceAndIdentity(){
        Window w;QVERIFY(w.windowTitle().contains("Pass v2.0 Plus"));
        QVERIFY(!w.findChild<QComboBox*>("randomAlgorithm"));
        auto dark=w.findChild<QAction*>("theme_dark");auto light=w.findChild<QAction*>("theme_light");QVERIFY(dark);QVERIFY(light);
        dark->trigger();QVERIFY(qApp->palette().color(QPalette::Window).lightness()<128);
        QCOMPARE(QSettings().value("ui/appearance").toString(),QString("dark"));
        w.show();QVERIFY(QTest::qWaitForWindowExposed(&w));
        QVERIFY(w.grab().save(QCoreApplication::applicationDirPath()+"/ui-plus-dark.png"));
        light->trigger();QVERIFY(qApp->palette().color(QPalette::Window).lightness()>128);
        QVERIFY(w.grab().save(QCoreApplication::applicationDirPath()+"/ui-plus-light.png"));
        auto spin=w.findChild<QSpinBox*>("characterLength");spin->setValue(20);spin->stepUp();QCOMPARE(spin->value(),21);
        auto tabs=w.findChild<QTabWidget*>("modeTabs");tabs->setCurrentIndex(1);QCOMPARE(tabs->count(),2);
    }
};
QTEST_MAIN(PlusTests)
#include "plus_test.moc"
