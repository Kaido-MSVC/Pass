// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Kaido-MSVC
#include "password.h"
#include "capture_protection.h"
#include "wordlist.h"
#include <QtWidgets>
void applyAppearance(QApplication& app, const QString& choice = "system");
#include "generation.h"
#include <atomic>
#include <memory>
#include <stdexcept>

class Window : public QMainWindow {
    QSpinBox *length, *batch, *wordCount, *wordBatch;
    QComboBox *separator;
    QTabWidget *tabs;
    QWidget *characterPage, *wordPage;
    QGroupBox *inspector;
    QLabel *lengthLabel;
    QVector<QCheckBox*> enabled;
    QVector<QSpinBox*> minima;
    QCheckBox *ambiguous, *visible, *autoClear;
    QLineEdit *custom, *excluded, *check;
    QPlainTextEdit *output;
    QLabel *ruleStatus, *entropyLabel, *analysisLabel, *matchLabel, *copyStatus;
    QProgressBar *strength;
    QPushButton *generateButton, *cancelButton;
    std::shared_ptr<Generation> job;
    QTimer pollTimer;
    SecretText secrets;

    QTimer clearTimer;
    SecretText copied;
    QSettings settings;
    bool generated = false;
    static QScrollArea* scrollRules(QWidget* contents) {
        auto scroll = new QScrollArea; scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true); scroll->setWidget(contents); return scroll;
    }
    pass::Rules rules() const {
        pass::Rules r; r.length=length->value(); r.upper=enabled[0]->isChecked(); r.lower=enabled[1]->isChecked(); r.digits=enabled[2]->isChecked(); r.symbols=enabled[3]->isChecked();
        r.minUpper=minima[0]->value(); r.minLower=minima[1]->value(); r.minDigits=minima[2]->value(); r.minSymbols=minima[3]->value();
        r.excludeAmbiguous=ambiguous->isChecked(); r.custom=custom->text(); r.excluded=excluded->text(); return r;
    }
    bool wordMode() const { return tabs->currentIndex() == 1; }
    pass::PassphraseRules phraseRules() const {
        pass::PassphraseRules r; r.words = wordCount->value();
        r.separator = separator->currentData().toString().at(0); return r;
    }
    QString validationError() const { return wordMode() ? pass::validate(phraseRules()) : pass::validate(rules()); }
    int intSetting(const QString& key, int fallback, int low, int high) const {
        bool ok = false; const int value = settings.value(key, fallback).toInt(&ok);
        return ok && value >= low && value <= high ? value : fallback;
    }
    void refresh() {
        const bool words = wordMode(); const QString error = validationError();
        ruleStatus->setText(error.isEmpty() ? (words && wordCount->value() < 6 ? QStringLiteral("規則可用；EFF 建議至少使用 6 個單詞。") : QStringLiteral("規則可用；密碼以安全亂數均勻取樣。")) : error);
        const bool dark=qApp->palette().color(QPalette::Window).lightness()<128;
        ruleStatus->setStyleSheet(error.isEmpty()?(dark?"color:#65cfb6":"color:#126c56"):(dark?"color:#f09191":"color:#a82929"));
        generateButton->setEnabled(error.isEmpty() && !job && !generationBusy.load());
        characterPage->setEnabled(!job); wordPage->setEnabled(!job && pass::wordlist().available());
        for (int i = 0; i < 4; ++i) minima[i]->setEnabled(enabled[i]->isChecked());
        if (words) {
            entropyLabel->setText(error.isEmpty() ? QStringLiteral("產生規則熵：約 %1 bits（分隔符不增加熵）").arg(pass::entropy(phraseRules()),0,'f',1) : QStringLiteral("請先修正產生規則。"));
            inspector->setTitle(QStringLiteral("單詞密碼格式檢查")); strength->hide();
            analysisLabel->setText(QStringLiteral("每個單詞獨立均勻抽樣，允許重複。手動輸入的片語不視為隨機產生。"));
            matchLabel->setText(check->text().isEmpty() ? QString() : pass::matches(check->text(),phraseRules()) ? QStringLiteral("符合目前詞表、詞數與分隔格式") : QStringLiteral("不符合目前詞表、詞數或分隔格式"));
        } else {
            entropyLabel->setText(error.isEmpty()?QStringLiteral("有效組合熵：約 %1 bits（依目前產生規則）").arg(pass::entropy(rules()),0,'f',1):QStringLiteral("請先修正產生規則。"));
            inspector->setTitle(QStringLiteral("密碼強度檢查")); strength->show();
            QString text = check->text(); const auto a=pass::analyze(text); wipeText(text);
            strength->setValue(a.score); strength->setFormat(a.label+"  %p%");
            analysisLabel->setText(a.feedback.join(QStringLiteral("\n")));
            matchLabel->setText(check->text().isEmpty()?QString():pass::matches(check->text(),rules())?QStringLiteral("符合目前產生規則"):QStringLiteral("不符合目前產生規則（長度／字元／最低數量）"));
        }
    }
    void saveRules() {
        settings.setValue("passphrase/words",wordCount->value());
        settings.setValue("passphrase/batch",wordBatch->value());
        settings.setValue("passphrase/separator",separator->currentData().toString());
        settings.setValue("length",length->value()); settings.setValue("batch",batch->value());
        for(int i=0;i<4;++i) {settings.setValue(QString("enabled%1").arg(i),enabled[i]->isChecked());settings.setValue(QString("minimum%1").arg(i),minima[i]->value());}
        settings.setValue("ambiguous",ambiguous->isChecked()); settings.setValue("custom",custom->text());settings.setValue("excluded",excluded->text());settings.setValue("autoClear",autoClear->isChecked());
    }
protected:
    bool event(QEvent* event) override {
        const bool handled = QMainWindow::event(event);
        if (event->type() == QEvent::WinIdChange) {
            // Do not call winId() here: destruction also emits WinIdChange,
            // and forcing a new HWND would recurse/recreate a dying window.
            if (const WId id = internalWinId())
                pass::capture::apply(reinterpret_cast<HWND>(id));
        }
        return handled;
    }
public:
#ifdef PASS_TESTING
    explicit Window(Generator generator = static_cast<Generator>(pass::generate), WordGenerator wordGenerator = static_cast<WordGenerator>(pass::generate)) {
#else
    Window() {
#endif
        setWindowTitle(QStringLiteral("Pass v2.0 Plus · 安全密碼產生器")); setWindowIcon(QIcon(":/app.ico"));
        const auto screen=QGuiApplication::primaryScreen();const QRect available=screen?screen->availableGeometry():QRect(0,0,1080,830);
        resize(qMin(1040,qMax(400,available.width()-32)),qMin(790,qMax(400,available.height()-32)));
        auto root=new QWidget; setCentralWidget(root); auto page=new QVBoxLayout(root); page->setContentsMargins(28,24,28,24); page->setSpacing(16);
        auto title=new QLabel(QStringLiteral("為每個帳號，產生獨一無二的密碼")); title->setObjectName("title"); page->addWidget(title);
        auto subtitle=new QLabel(QStringLiteral("BCryptGenRandom · Windows 安全亂數 · 本機離線運作 · 不儲存密碼")); subtitle->setProperty("muted",true); page->addWidget(subtitle);
        auto columns=new QHBoxLayout; columns->setSpacing(20); page->addLayout(columns,1);
        auto left=new QGroupBox(QStringLiteral("產生規則"));left->setObjectName("rulesCard"); auto form=new QVBoxLayout(left);form->setSpacing(12);columns->addWidget(left,1);
        tabs = new QTabWidget; tabs->setObjectName("modeTabs");tabs->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Ignored);tabs->setMinimumHeight(60);form->addWidget(tabs,1);
        characterPage = new QWidget;characterPage->setObjectName("settingsPage"); auto charForm = new QVBoxLayout(characterPage);
        charForm->setSpacing(12); tabs->addTab(scrollRules(characterPage),QStringLiteral("隨機字元密碼"));
        auto numbers=new QFormLayout; length=new QSpinBox;length->setObjectName("characterLength");length->setRange(4,128);length->setValue(intSetting("length",20,4,128));batch=new QSpinBox;batch->setObjectName("characterBatch");batch->setRange(1,100);batch->setValue(intSetting("batch",1,1,100));numbers->addRow(QStringLiteral("密碼長度"),length);numbers->addRow(QStringLiteral("產生數量"),batch);charForm->addLayout(numbers);
        auto grid=new QGridLayout;grid->addWidget(new QLabel(QStringLiteral("啟用字元類型")),0,0);grid->addWidget(new QLabel(QStringLiteral("最低數量")),0,1);
        const QStringList names={QStringLiteral("大寫字母  A–Z"),QStringLiteral("小寫字母  a–z"),QStringLiteral("數字  0–9"),QStringLiteral("符號  !@#$…")};
        for(int i=0;i<4;++i) { auto box=new QCheckBox(names[i]);box->setChecked(settings.value(QString("enabled%1").arg(i),true).toBool());enabled+=box;auto spin=new QSpinBox;spin->setObjectName(QString("minimum%1").arg(i));spin->setRange(0,8);spin->setValue(intSetting(QString("minimum%1").arg(i),1,0,8));spin->setEnabled(box->isChecked());minima+=spin;grid->addWidget(box,i+1,0);grid->addWidget(spin,i+1,1);connect(box,&QCheckBox::toggled,spin,&QWidget::setEnabled); }
        charForm->addLayout(grid);ambiguous=new QCheckBox(QStringLiteral("排除易混淆字元  I l 1 O 0 o |"));ambiguous->setChecked(settings.value("ambiguous",true).toBool());charForm->addWidget(ambiguous);
        auto chars=new QFormLayout;custom=new QLineEdit(settings.value("custom").toString());custom->setPlaceholderText(QStringLiteral("加入字元池，例如：~<>"));excluded=new QLineEdit(settings.value("excluded").toString());excluded->setPlaceholderText(QStringLiteral("不要出現的字元"));custom->setMaxLength(256);excluded->setMaxLength(256);chars->addRow(QStringLiteral("自訂字元"),custom);chars->addRow(QStringLiteral("排除字元"),excluded);charForm->addLayout(chars);
        auto note=new QLabel(QStringLiteral("自訂字元會加入字元池；關閉全部類型即可只用自訂字元。內建符號新增 ~ < > |，不含空白、引號、反斜線與反引號；自訂仍限可見 ASCII。每類最低數量 0–8。"));note->setWordWrap(true);note->setProperty("muted",true);charForm->addWidget(note);
        charForm->addStretch();
        wordPage = new QWidget;wordPage->setObjectName("settingsPage"); auto wordForm = new QVBoxLayout(wordPage); wordForm->setSpacing(12);
        tabs->addTab(scrollRules(wordPage),QStringLiteral("隨機單詞密碼"));
        auto source = new QLabel(QStringLiteral("EFF Long Wordlist · 7,776 詞 · 離線")); source->setWordWrap(true); wordForm->addWidget(source);
        auto wordNumbers = new QFormLayout;
        wordCount = new QSpinBox; wordCount->setObjectName("wordCount"); wordCount->setRange(4,12); wordCount->setValue(intSetting("passphrase/words",6,4,12));
        wordBatch = new QSpinBox; wordBatch->setObjectName("wordBatch"); wordBatch->setRange(1,100); wordBatch->setValue(intSetting("passphrase/batch",1,1,100));
        separator = new QComboBox; separator->setObjectName("separator");
        separator->addItem(QStringLiteral("連字號  -"),QStringLiteral("-")); separator->addItem(QStringLiteral("底線  _"),QStringLiteral("_")); separator->addItem(QStringLiteral("空白"),QStringLiteral(" "));
        const int savedSeparator = separator->findData(settings.value("passphrase/separator",QStringLiteral("-")));
        separator->setCurrentIndex(savedSeparator < 0 ? 0 : savedSeparator);
        wordNumbers->addRow(QStringLiteral("單詞數量"),wordCount); wordNumbers->addRow(QStringLiteral("分隔符號"),separator); wordNumbers->addRow(QStringLiteral("產生數量"),wordBatch); wordForm->addLayout(wordNumbers);
        auto wordNote = new QLabel(QStringLiteral("預設 6 詞；保留官方英文小寫，允許重複單詞。請先確認網站接受的字元與長度，勿截斷產生結果。")); wordNote->setWordWrap(true); wordForm->addWidget(wordNote);
        if (!pass::wordlist().available()) { auto failure = new QLabel(pass::wordlist().error); failure->setWordWrap(true); failure->setStyleSheet("color:#c54141"); wordForm->addWidget(failure); }
        wordForm->addStretch();
        ruleStatus=new QLabel;ruleStatus->setWordWrap(true);form->addWidget(ruleStatus);entropyLabel=new QLabel;entropyLabel->setWordWrap(true);form->addWidget(entropyLabel);form->addStretch();
        generateButton=new QPushButton(QStringLiteral("產生密碼"));generateButton->setObjectName("primary");generateButton->setMinimumHeight(44);cancelButton=new QPushButton(QStringLiteral("取消產生"));cancelButton->setEnabled(false);form->addWidget(generateButton);form->addWidget(cancelButton);
        auto right=new QWidget;auto rightLayout=new QVBoxLayout(right);rightLayout->setContentsMargins(0,0,0,0);columns->addWidget(scrollRules(right),1);
        auto results=new QGroupBox(QStringLiteral("產生結果"));auto resultLayout=new QVBoxLayout(results);rightLayout->addWidget(results,1);
        output=new QPlainTextEdit;output->setReadOnly(true);output->setUndoRedoEnabled(false);output->setPlaceholderText(QStringLiteral("按下「產生密碼」開始"));output->setFont(QFont("Consolas",12));output->setContextMenuPolicy(Qt::NoContextMenu);resultLayout->addWidget(output);
        lengthLabel = new QLabel; lengthLabel->setObjectName("resultLength"); resultLayout->addWidget(lengthLabel);
        visible=new QCheckBox(QStringLiteral("顯示密碼"));visible->setChecked(false);resultLayout->addWidget(visible);autoClear=new QCheckBox(QStringLiteral("複製後 30 秒清除剪貼簿"));autoClear->setChecked(settings.value("autoClear",true).toBool());resultLayout->addWidget(autoClear);
        auto buttons=new QHBoxLayout;auto copy=new QPushButton(QStringLiteral("複製全部"));auto erase=new QPushButton(QStringLiteral("清除結果"));buttons->addWidget(copy);buttons->addWidget(erase);resultLayout->addLayout(buttons);copyStatus=new QLabel;copyStatus->setWordWrap(true);resultLayout->addWidget(copyStatus);
        inspector=new QGroupBox(QStringLiteral("密碼強度檢查"));auto inspectLayout=new QVBoxLayout(inspector);rightLayout->addWidget(inspector);
        check=new QLineEdit;check->setMaxLength(128);check->setEchoMode(QLineEdit::Password);check->setPlaceholderText(QStringLiteral("輸入密碼，或使用第一筆產生結果"));inspectLayout->addWidget(check);strength=new QProgressBar;strength->setRange(0,100);inspectLayout->addWidget(strength);matchLabel=new QLabel;matchLabel->setWordWrap(true);inspectLayout->addWidget(matchLabel);analysisLabel=new QLabel;analysisLabel->setWordWrap(true);analysisLabel->setProperty("muted",true);inspectLayout->addWidget(analysisLabel);
        clearTimer.setParent(this);clearTimer.setObjectName("clipboardTimer");clearTimer.setSingleShot(true);
        connect(&clearTimer,&QTimer::timeout,this,[this]{clearCopied();});
        connect(copy,&QPushButton::clicked,this,[this]{
            if(!generated)return;
            QString text=secrets.text();QApplication::clipboard()->setText(text);
            copied.reserve(static_cast<size_t>(text.size()));copied.append(text);wipeText(text);
            clearTimer.stop();if(autoClear->isChecked())clearTimer.start(30000);
            copyStatus->setText(autoClear->isChecked()?QStringLiteral("已複製；30 秒後只清除仍相同的內容。"):QStringLiteral("已複製。"));
        });
        connect(erase,&QPushButton::clicked,this,[this]{abandonJob();clearResults();clearCopied();refresh();});
        connect(visible,&QCheckBox::toggled,this,[this](bool show){check->setEchoMode(show?QLineEdit::Normal:QLineEdit::Password);if(generated)displayResults();});
#ifdef PASS_TESTING
        connect(generateButton,&QPushButton::clicked,this,[this,generator,wordGenerator]{
#else
        connect(generateButton,&QPushButton::clicked,this,[this]{
#endif
            saveRules();clearResults();
            try {
#ifdef PASS_TESTING
                job = wordMode() ? startGeneration(phraseRules(),wordBatch->value(),wordGenerator) : startGeneration(rules(),batch->value(),generator);
#else
                job = wordMode() ? startGeneration(phraseRules(),wordBatch->value()) : startGeneration(rules(),batch->value());
#endif
            }
            catch(const std::exception&) {ruleStatus->setText(QStringLiteral("無法啟動背景工作。"));return;}
            if(!job){ruleStatus->setText(QStringLiteral("安全亂數服務仍在處理先前工作，請稍後重試。"));return;}
            characterPage->setEnabled(false);wordPage->setEnabled(false);generateButton->setEnabled(false);cancelButton->setEnabled(true);ruleStatus->setText(QStringLiteral("正在產生…"));
        });
        connect(cancelButton,&QPushButton::clicked,this,[this]{abandonJob();refresh();ruleStatus->setText(QStringLiteral("已取消；不會顯示本次結果。"));});
        pollTimer.setInterval(25);
        connect(&pollTimer,&QTimer::timeout,this,[this]{
            if(!job){generateButton->setEnabled(!generationBusy.load() && validationError().isEmpty());return;}
            if(std::chrono::steady_clock::now()-job->started>=std::chrono::seconds(10)){
                abandonJob();refresh();ruleStatus->setText(QStringLiteral("等待安全亂數服務逾時，已停止接收結果。"));return;
            }
            if(!job->done.load())return;
            auto completed=std::move(job);cancelButton->setEnabled(false);
            std::lock_guard<std::mutex> lock(completed->mutex);
            refresh();
            if(!completed->error.isEmpty()){ruleStatus->setText(completed->error);return;}
            secrets.swap(completed->secret);generated=true;displayResults();
            QString text=secrets.text();QString first=text.section('\n',0,0);check->setText(first);wipeText(first);wipeText(text);copyStatus->clear();
        });pollTimer.start();
        connect(length,&QSpinBox::valueChanged,this,[this]{refresh();});for(auto spin:minima)connect(spin,&QSpinBox::valueChanged,this,[this]{refresh();});for(auto box:enabled)connect(box,&QCheckBox::toggled,this,[this]{refresh();});connect(ambiguous,&QCheckBox::toggled,this,[this]{refresh();});for(auto field:{custom,excluded,check})connect(field,&QLineEdit::textChanged,this,[this]{refresh();});
        connect(wordCount,&QSpinBox::valueChanged,this,[this]{refresh();});
        connect(separator,&QComboBox::currentIndexChanged,this,[this]{refresh();});
        connect(tabs,&QTabWidget::currentChanged,this,[this]{
            abandonJob(); clearResults(); clearCopied(); visible->setChecked(false);
            check->setMaxLength(wordMode() && pass::wordlist().available() ? 12 * pass::wordlist().maxLength + 11 : 128);
            refresh();
        });
        auto themeMenu=menuBar()->addMenu(QStringLiteral("外觀"));
        auto themeGroup=new QActionGroup(this);themeGroup->setExclusive(true);
        QString choice=settings.value("ui/appearance","system").toString();
        if(choice!="light" && choice!="dark")choice="system";
        const QStringList labels={QStringLiteral("跟隨 Windows"),QStringLiteral("淺色"),QStringLiteral("深色")};
        const QStringList values={"system","light","dark"};
        for(int i=0;i<values.size();++i){auto action=themeMenu->addAction(labels[i]);action->setObjectName("theme_"+values[i]);
            action->setCheckable(true);action->setData(values[i]);themeGroup->addAction(action);action->setChecked(values[i]==choice);
            connect(action,&QAction::triggered,this,[this,value=values[i]]{settings.setValue("ui/appearance",value);applyAppearance(*qApp,value);refresh();});}
        connect(qApp->styleHints(),&QStyleHints::colorSchemeChanged,this,[this]{
            const auto value=settings.value("ui/appearance","system").toString();
            if(value!="light" && value!="dark"){applyAppearance(*qApp,"system");refresh();}});
        applyAppearance(*qApp,choice);
        auto helpMenu=menuBar()->addMenu(QStringLiteral("說明"));
        auto aboutAction=helpMenu->addAction(QStringLiteral("關於 Pass"));aboutAction->setObjectName("about_pass");
        connect(aboutAction,&QAction::triggered,this,[this]{
            QMessageBox::about(this,QStringLiteral("關於 Pass"),QStringLiteral(
                "<b>Pass v2.0 Plus · 安全密碼產生器</b><br>"
                "作者：Kaido-MSVC<br>Copyright (c) 2026 Kaido-MSVC<br>本程式以 MIT License 釋出。<br><br>"
                "第三方元件：<br>"
                "· Qt 6.12.0 — LGPLv3（動態連結）<br>"
                "· zxcvbn-c — MIT<br>"
                "· EFF Long Wordlist — CC BY 4.0<br><br>"
                "完整條文見程式資料夾的 LICENSE.txt、THIRD_PARTY_NOTICES.md 與 LICENSES。"));});
        auto aboutQtAction=helpMenu->addAction(QStringLiteral("關於 Qt"));aboutQtAction->setObjectName("about_qt");
        connect(aboutQtAction,&QAction::triggered,this,[this]{QMessageBox::aboutQt(this);});
        refresh();
        // Create the HWND and protect it before the first show().
        (void)winId();
    }
    static QString masked(const QString& text) {QString value=text;for(QChar& c:value)if(c!='\n')c=QChar(0x2022);return value;}
    void abandonJob() { if(job){job->abandon();job.reset();} cancelButton->setEnabled(false); }
    void clearCopied() {
        clearTimer.stop();QString expected=copied.text();QString actual=QApplication::clipboard()->text();
        if(!expected.isEmpty() && actual==expected)QApplication::clipboard()->clear();
        wipeText(expected);wipeText(actual);copied.clear();
    }
    void clearResults() {
        generated=false;output->clear();check->clear();secrets.clear();copyStatus->clear();lengthLabel->clear();
    }
    void displayResults() {
        QString text=secrets.text();QString display=visible->isChecked()?QString(text.constData(),text.size()):masked(text);
        output->setPlainText(display);
        const qsizetype firstLength = text.indexOf('\n') < 0 ? text.size() : text.indexOf('\n');
        lengthLabel->setText(QStringLiteral("第一筆長度：%1 字元").arg(firstLength));
        wipeText(display);wipeText(text);
    }
    ~Window() override {pollTimer.stop();abandonJob();saveRules();clearResults();clearCopied();}
};
void applyAppearance(QApplication& app, const QString& choice) {
    // Leave system detection enabled even when the user explicitly chooses a theme.
    const bool dark=choice=="dark" || (choice=="system" && app.styleHints()->colorScheme()==Qt::ColorScheme::Dark);
    app.setStyle("Fusion");
    app.setFont(QFont("Microsoft JhengHei UI",10));
    QPalette palette;
    const QColor bg(dark?"#202630":"#f3f6fa"), panel(dark?"#29323e":"#ffffff"), text(dark?"#e4ebf3":"#18283d"), border(dark?"#536174":"#cbd5e1");
    palette.setColor(QPalette::Light,border.lighter());palette.setColor(QPalette::Mid,border);palette.setColor(QPalette::Dark,border.darker());
    palette.setColor(QPalette::PlaceholderText,QColor(dark?"#abb8c9":"#596b80"));
    palette.setColor(QPalette::Window,bg);palette.setColor(QPalette::WindowText,text);
    palette.setColor(QPalette::Base,panel);palette.setColor(QPalette::AlternateBase,bg);palette.setColor(QPalette::Text,text);
    palette.setColor(QPalette::Button,panel);palette.setColor(QPalette::ButtonText,text);
    palette.setColor(QPalette::Highlight,QColor("#176e65"));palette.setColor(QPalette::HighlightedText,Qt::white);
    palette.setColor(QPalette::ToolTipBase,panel);palette.setColor(QPalette::ToolTipText,text);
    palette.setColor(QPalette::Disabled,QPalette::Text,QColor(dark?"#8794a4":"#718096"));
    app.setPalette(palette);
    // Keep spin boxes on the native Fusion paint path, including arrows and step states.
    const QString settings=dark?"#252e39":"#edf1f5";
    const QString hover=dark?"#39495a":"#e3ecef", pressed=dark?"#172c32":"#cbdedc";
    const QString disabled=dark?"#303946":"#e5e9ee", muted=dark?"#9aa7b7":"#647387";
    app.setStyleSheet(QStringLiteral(R"(
QMainWindow{background:%1;color:%3}
QGroupBox{background:%2;border:1px solid %4;border-radius:10px;margin-top:14px;padding:14px}
QGroupBox#rulesCard{background:%5}
QGroupBox::title{subcontrol-origin:margin;left:12px;padding:0 5px;color:%3;font-weight:bold}
QLabel#title{font-size:22px;font-weight:bold;color:%3}
QLabel[muted="true"]{color:%9}
QWidget#settingsPage,QTabWidget#modeTabs QScrollArea,QTabWidget#modeTabs QScrollArea > QWidget{background:%5}
QTabWidget#modeTabs::pane{border:0;background:%5;padding-top:10px}
QLineEdit,QPlainTextEdit{border:1px solid %4;border-radius:6px;padding:7px;background:%2;color:%3}
QPushButton{border:1px solid %4;border-radius:7px;padding:9px;background:%2;color:%3}
QPushButton:hover:enabled{background:%6;border-color:#368b80}
QPushButton:focus{border-color:#368b80}
QPushButton:pressed:enabled{background:%7;border-color:#176e65;padding-top:10px;padding-bottom:8px}
QPushButton:disabled{background:%8;color:%9;border-color:%4}
QPushButton#primary{background:#176e65;color:white;border:0;font-weight:bold}
QPushButton#primary:hover:enabled{background:#1b7b70}
QPushButton#primary:pressed:enabled{background:#10564f;padding-top:10px;padding-bottom:8px}
QPushButton#primary:disabled{background:%8;color:%9}
QProgressBar{border:0;background:%2;border-radius:6px;height:22px;text-align:center}
QProgressBar::chunk{background:#32a68c;border-radius:6px}
QSpinBox{min-height:32px}
QTabBar::tab{border:1px solid %4;border-radius:0;padding:7px 12px;margin:0;background:%2;color:%3}
QTabBar::tab:first{border-top-left-radius:6px;border-bottom-left-radius:6px}
QTabBar::tab:last{border-top-right-radius:6px;border-bottom-right-radius:6px}
QTabBar::tab:hover:!selected:enabled{background:%6;border-color:#368b80}
QTabBar::tab:selected{background:#176e65;color:white;border-color:#176e65;font-weight:bold}
QTabBar::tab:selected:hover{background:#1b7b70}
QTabBar::tab:disabled{background:%8;color:%9;border-color:%4}
QMenuBar,QMenu{background:%2;color:%3}
QMenu::item:selected,QMenuBar::item:selected{background:#176e65;color:white}
)").arg(bg.name(),panel.name(),text.name(),border.name(),settings,hover,pressed,disabled,muted));
}
#ifndef PASS_UI_TEST
int main(int argc,char** argv) {
    QApplication app(argc,argv);QCoreApplication::setOrganizationName("PassLocal");QCoreApplication::setApplicationName("Pass");QCoreApplication::setApplicationVersion("2.0 Plus");
    applyAppearance(app);
    Window window;window.show();
    if(app.arguments().contains("--word-tab"))window.findChild<QTabWidget*>("modeTabs")->setCurrentIndex(1);
    const QStringList args=app.arguments();if(args.contains("--smoke-test")){if(!pass::wordlist().available())return 2;QTimer::singleShot(250,&app,&QApplication::quit);}
    int screenshot=args.indexOf("--screenshot");if(screenshot>=0 && screenshot+1<args.size())QTimer::singleShot(500,&app,[&]{window.grab().save(args[screenshot+1]);app.quit();});
    return app.exec();
}
#endif
