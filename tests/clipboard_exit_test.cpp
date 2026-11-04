// Cross-process regression: only public fixtures are used; clipboard text is never logged.
#define PASS_UI_TEST
#include "../src/main.cpp"
#include <QtTest>
#include <iostream>

namespace {
QString fixture() { return QStringLiteral("Pass-Clipboard-Test-ONLY-123!\n測試-日本語-Ω"); }
QString generateFixture(const pass::Rules&, const std::function<bool()>&) { return fixture(); }
QString externalFixture() { return QStringLiteral("External-Clipboard-Test-ONLY"); }

// Runs before QApplication exists, reading the native clipboard in a fresh process.
int readFixture(bool external, bool empty) {
    bool opened=false;
    for(int i=0;i<100 && !(opened=OpenClipboard(nullptr));++i)Sleep(10);
    if(!opened)return 20;
    HANDLE handle=GetClipboardData(CF_UNICODETEXT);
    bool same=empty && !handle;
    if(handle){
        auto text=static_cast<const wchar_t*>(GlobalLock(handle));
        const QString expected=external?externalFixture():fixture();
        const SIZE_T bytes=(static_cast<SIZE_T>(expected.size())+1)*sizeof(wchar_t);
        same=!empty && text && GlobalSize(handle)>=bytes &&
             text[expected.size()]==0 &&
             std::memcmp(text,expected.utf16(),bytes-sizeof(wchar_t))==0;
        if(text)GlobalUnlock(handle);
    }
    if(same && !external && !empty){
        for(auto name:{L"CanIncludeInClipboardHistory",L"CanUploadToCloudClipboard"}){
            auto h=GetClipboardData(RegisterClipboardFormatW(name));
            auto flag=h?static_cast<const DWORD*>(GlobalLock(h)):nullptr;
            same=flag && GlobalSize(h)>=sizeof(DWORD) && *flag==0 && same;
            if(flag)GlobalUnlock(h);
        }
        same=IsClipboardFormatAvailable(RegisterClipboardFormatW(L"ExcludeClipboardContentFromMonitorProcessing")) && same;
    }
    CloseClipboard();
    return same?0:21;
}

int writeFixture(QApplication& app, const QString& mode, bool autoClear, const QString& replacement) {
    QTemporaryDir settingsDir;
    if(!settingsDir.isValid())return 30;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settingsDir.path());
    QCoreApplication::setOrganizationName("PassClipboardTests");
    QCoreApplication::setApplicationName("Writer");
    QSettings settings;settings.setValue("ui/language","zh-TW");settings.setValue("autoClear",autoClear);
    auto windowOwner=std::make_unique<Window>(generateFixture);
    Window& window=*windowOwner;
    window.findChild<QSpinBox*>("characterLength")->setValue(128);
    // Match by translated text rather than relying on a settings key.
    for(auto cb:window.findChildren<QCheckBox*>())
        if(cb->text()==pass::uiText(QStringLiteral("複製後 30 秒清除剪貼簿")))cb->setChecked(autoClear);
    window.show();
    QTimer deadline;deadline.setSingleShot(true);
    QObject::connect(&deadline,&QTimer::timeout,&app,[&]{app.exit(31);});deadline.start(5000);
    QTimer ready;
    QObject::connect(&ready,&QTimer::timeout,&window,[&]{
        if(!window.findChild<QPushButton*>("primary")->isEnabled() ||
           window.findChild<QPlainTextEdit*>()->toPlainText().isEmpty())return;
        ready.stop();
        QPushButton* copy=nullptr;
        for(auto b:window.findChildren<QPushButton*>())
            if(b->text()==pass::uiText(QStringLiteral("複製全部")))copy=b;
        if(!copy){app.exit(32);return;}
        const DWORD previousSequence=GetClipboardSequenceNumber();
        copy->click();
        // Do not read text before exit: the first text read must be in a fresh process.
        if(!IsClipboardFormatAvailable(CF_UNICODETEXT) ||
           GetClipboardOwner()!=reinterpret_cast<HWND>(window.winId()) ||
           GetClipboardSequenceNumber()==previousSequence){app.exit(33);return;}
        std::cout<<"copied"<<std::endl;
        if(replacement=="external")QApplication::clipboard()->setText(externalFixture());
        if(replacement=="clear")QApplication::clipboard()->clear();
        if(mode=="wm-close")PostMessageW(reinterpret_cast<HWND>(window.winId()),WM_CLOSE,0,0);
        else if(mode=="system-close")PostMessageW(reinterpret_cast<HWND>(window.winId()),WM_SYSCOMMAND,SC_CLOSE,0);
        else if(mode=="destroy"){windowOwner.reset();app.quit();}
        else if(mode=="quit")app.quit();
        else window.close();
    });
    QTimer::singleShot(0,&window,[&]{window.findChild<QPushButton*>("primary")->click();ready.start(1);});
    return app.exec(); // Window destructor then QApplication destructor both run before reader starts.
}
}

class ClipboardExitTests:public QObject {
    Q_OBJECT
private slots:
    void survivesExit_data(){
        QTest::addColumn<QString>("mode");QTest::addColumn<bool>("autoClear");QTest::addColumn<QString>("replacement");
        for(auto mode:{"close","wm-close","system-close","quit","destroy"})
            for(bool clear:{false,true})
                QTest::newRow(qPrintable(QString("%1-auto%2").arg(mode).arg(clear)))<<QString(mode)<<clear<<QString("none");
        QTest::newRow("external-copy-survives")<<QString("wm-close")<<true<<QString("external");
        QTest::newRow("external-clear-respected")<<QString("wm-close")<<true<<QString("clear");
    }
    void survivesExit(){
        QFETCH(QString,mode);QFETCH(bool,autoClear);QFETCH(QString,replacement);
        for(int cycle=0;cycle<10;++cycle){
            QProcess writer;
            writer.start(QCoreApplication::applicationFilePath(),{"--writer",mode,autoClear?"1":"0",replacement});
            QTRY_VERIFY_WITH_TIMEOUT(writer.state()==QProcess::NotRunning,10000);
            QVERIFY(writer.exitStatus()==QProcess::NormalExit);
            QVERIFY2(writer.exitCode()==0,qPrintable(QString("Writer exit code %1 (no clipboard text logged)").arg(writer.exitCode())));
            QVERIFY(writer.readAllStandardOutput().contains("copied"));
            QProcess reader;
            reader.start(QCoreApplication::applicationFilePath(),{"--reader",replacement});
            QTRY_VERIFY_WITH_TIMEOUT(reader.state()==QProcess::NotRunning,5000);
            QVERIFY(reader.exitStatus()==QProcess::NormalExit);
            QVERIFY2(reader.exitCode()==0,"Independent reader mismatch after writer exited (no clipboard text logged)");
        }
    }
};
int main(int argc,char** argv){
    if(argc>=2 && std::strcmp(argv[1],"--reader")==0)
        return readFixture(argc>=3 && std::strcmp(argv[2],"external")==0,argc>=3 && std::strcmp(argv[2],"clear")==0);
    QApplication app(argc,argv);
    if(argc>=5 && std::strcmp(argv[1],"--writer")==0)
        return writeFixture(app,QString::fromLocal8Bit(argv[2]),std::strcmp(argv[3],"1")==0,QString::fromLocal8Bit(argv[4]));
    ClipboardExitTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "clipboard_exit_test.moc"
