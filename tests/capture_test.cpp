#define PASS_UI_TEST
#include "../src/main.cpp"
#include <QtTest>

namespace {
QVector<DWORD> requests;
DWORD firstError = ERROR_SUCCESS;
DWORD retryError = ERROR_SUCCESS;
BOOL WINAPI fakeSet(HWND, DWORD affinity) {
    requests.append(affinity);
    const DWORD error = requests.size() == 1 ? firstError : retryError;
    SetLastError(error);
    return error == ERROR_SUCCESS;
}
class RecreatedWindow : public Window {
public:
    void recreate() { destroy(); create(); }
};
}

class CaptureTests : public QObject {
    Q_OBJECT
    QTemporaryDir settings;
private slots:
    void initTestCase() {
        QVERIFY(settings.isValid());
        QCoreApplication::setOrganizationName("PassCaptureTests");
        QCoreApplication::setApplicationName("Capture");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
        qInfo() << "Windows version:" << QOperatingSystemVersion::current().majorVersion()
                << QOperatingSystemVersion::current().minorVersion()
                << QOperatingSystemVersion::current().microVersion();
    }
    void init() { requests.clear(); firstError = retryError = ERROR_SUCCESS; }
    void successfulExclusion() {
        QCOMPARE(pass::capture::apply(nullptr, fakeSet, true), pass::capture::Protection::Excluded);
        QCOMPARE(requests, QVector<DWORD>{0x11});
    }
    void oldWindowsSemantics() {
        QCOMPARE(pass::capture::apply(nullptr, fakeSet, false), pass::capture::Protection::MonitorOnly);
        QCOMPARE(requests, QVector<DWORD>{0x11});
    }
    void unsupportedFlagFallback_data() {
        QTest::addColumn<DWORD>("error");
        QTest::newRow("invalid-parameter") << DWORD(ERROR_INVALID_PARAMETER);
        QTest::newRow("not-supported") << DWORD(ERROR_NOT_SUPPORTED);
        QTest::newRow("not-implemented") << DWORD(ERROR_CALL_NOT_IMPLEMENTED);
    }
    void unsupportedFlagFallback() {
        QFETCH(DWORD,error); firstError=error;
        QCOMPARE(pass::capture::apply(nullptr,fakeSet,true),pass::capture::Protection::MonitorOnly);
        QCOMPARE(requests,(QVector<DWORD>{0x11,WDA_MONITOR}));
    }
    void fallbackFailureContinues() {
        firstError=ERROR_INVALID_PARAMETER; retryError=ERROR_ACCESS_DENIED;
        QCOMPARE(pass::capture::apply(nullptr,fakeSet,true),pass::capture::Protection::Unprotected);
        QCOMPARE(requests.size(),2);
    }
    void arbitraryFailureNoRetry() {
        firstError=ERROR_ACCESS_DENIED;
        QCOMPARE(pass::capture::apply(nullptr,fakeSet,true),pass::capture::Protection::Unprotected);
        QCOMPARE(requests,QVector<DWORD>{0x11});
    }
    void mainWindowLifecycle() {
        RecreatedWindow w;
        auto verify=[&] {
            QVERIFY(w.internalWinId());
            DWORD affinity=0;
            QVERIFY(GetWindowDisplayAffinity(reinterpret_cast<HWND>(w.internalWinId()),&affinity));
            QCOMPARE(affinity,pass::capture::excludeFromCapture);
        };
        // Constructor creates HWND and protects it while still hidden.
        QVERIFY(!w.isVisible()); verify();
        w.show(); QVERIFY(QTest::qWaitForWindowExposed(&w)); verify();
        w.hide(); w.show(); verify();
        w.showMinimized(); QTest::qWait(50); w.showNormal(); verify();
        w.recreate(); verify(); w.show(); verify();
    }
    void desktopCaptureComparison() {
        Window w;
        w.setWindowFlag(Qt::WindowStaysOnTopHint);
        w.resize(650,500); w.move(80,80);
        auto marker=new QWidget(w.centralWidget());
        marker->setGeometry(80,80,160,100);
        marker->setAutoFillBackground(true);
        QPalette palette=marker->palette();
        const QColor color(231,17,193);
        palette.setColor(QPalette::Window,color);marker->setPalette(palette);
        marker->show();marker->raise();w.show();w.raise();
        QVERIFY(QTest::qWaitForWindowExposed(&w)); QTest::qWait(500);
        const QPoint origin=marker->mapToGlobal(QPoint(30,30));
        auto capture=[&] { return w.screen()->grabWindow(0,origin.x(),origin.y(),32,32).toImage().convertToFormat(QImage::Format_RGB32); };
        auto matches=[&](const QImage& img) {
            int count=0;
            for(int y=0;y<img.height();++y)for(int x=0;x<img.width();++x)
                if(img.pixelColor(x,y)==color)++count;
            return count;
        };
        const auto protectedImage=capture(); QVERIFY(!protectedImage.isNull());
        // Positive control: temporarily remove affinity in this TEST process only.
        QVERIFY(SetWindowDisplayAffinity(reinterpret_cast<HWND>(w.winId()),WDA_NONE));
        QTest::qWait(250);const auto control=capture(); QVERIFY(!control.isNull());
        QVERIFY(SetWindowDisplayAffinity(reinterpret_cast<HWND>(w.winId()),0x11));
        qInfo() << "Desktop region capture marker pixels: protected=" << matches(protectedImage)
                << "unprotected control=" << matches(control) << "pixels=" << control.width()*control.height();
        QVERIFY(control.save(QCoreApplication::applicationDirPath()+"/capture-control.png"));
        QVERIFY(protectedImage.save(QCoreApplication::applicationDirPath()+"/capture-protected.png"));
        QVERIFY(matches(control)>control.width()*control.height()*9/10);
        QCOMPARE(matches(protectedImage),0);
    }
};
QTEST_MAIN(CaptureTests)
#include "capture_test.moc"
