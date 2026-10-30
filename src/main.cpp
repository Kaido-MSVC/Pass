#include "password.h"
#include "localization.h"
#include "wordlist.h"
#include <QtWidgets>
#include "generation.h"
#include <atomic>
#include <memory>
#include <stdexcept>
#include <cstring>
#include <ole2.h>
#include <openssl/crypto.h>
#include "capture_protection.h"
#include "pass_version.h"
#include <chrono>
#include <thread>

void applyAppearance(QApplication& app, const QString& choice = QStringLiteral("system"));
#pragma comment(lib, "Ole32.lib")
#ifdef PASS_TESTING
static auto clearClipboardContents=&::EmptyClipboard;
#else
constexpr auto clearClipboardContents=&::EmptyClipboard;
#endif

class Window : public QMainWindow {
    QSpinBox *length, *batch, *wordCount, *wordBatch;
    QComboBox *separator, *algorithm, *language, *appearance;
    QMenu *algorithmMenu, *languageMenu, *appearanceMenu, *helpMenu;
    QAction *aboutAction, *aboutQtAction;
    QElapsedTimer clipboardElapsed;
    DWORD clipboardSequence = 0;
    bool writingClipboard = false;
    bool clipboardWiped = false;
    int clipboardRetryCount = 0;
    QElapsedTimer clipboardRetryElapsed;
    QTimer countdownTimer;
    QTabWidget *tabs;
    QWidget *characterPage, *wordPage;
    QGroupBox *inspector;
    QLabel *lengthLabel, *algorithmInfo;
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
    QTranslator qtTranslator;
    bool generated = false;
    QString operationSource;
    void showOperation(const QString& message){operationSource=message;ruleStatus->setText(pass::uiText(message));}
    static QScrollArea* scrollRules(QWidget* contents) {
        auto scroll = new QScrollArea; scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true); scroll->setWidget(contents); return scroll;
    }
    pass::Rules rules() const {
        pass::Rules r; r.algorithm=selectedAlgorithm(); r.length=length->value(); r.upper=enabled[0]->isChecked(); r.lower=enabled[1]->isChecked(); r.digits=enabled[2]->isChecked(); r.symbols=enabled[3]->isChecked();
        r.minUpper=minima[0]->value(); r.minLower=minima[1]->value(); r.minDigits=minima[2]->value(); r.minSymbols=minima[3]->value();
        r.excludeAmbiguous=ambiguous->isChecked(); r.custom=custom->text(); r.excluded=excluded->text(); return r;
    }
    pass::RandomAlgorithm selectedAlgorithm() const {
        pass::RandomAlgorithm value;
        if (!pass::parseAlgorithm(algorithm->currentData().toString(), value))
            return static_cast<pass::RandomAlgorithm>(-1);
        return value;
    }
    bool wordMode() const { return tabs->currentIndex() == 1; }
    pass::PassphraseRules phraseRules() const {
        pass::PassphraseRules r; r.algorithm=selectedAlgorithm(); r.words = wordCount->value();
        r.separator = separator->currentData().toString().at(0); return r;
    }
    QString validationError() const { if (!pass::algorithmName(selectedAlgorithm())) return pass::uiText(QStringLiteral("已儲存的亂數演算法無效，請明確選擇演算法。")); return pass::uiText(wordMode() ? pass::validate(phraseRules()) : pass::validate(rules())); }
    int intSetting(const QString& key, int fallback, int low, int high) const {
        bool ok = false; const int value = settings.value(key, fallback).toInt(&ok);
        return ok && value >= low && value <= high ? value : fallback;
    }
    void refresh() {
        const bool words = wordMode(); const QString error = validationError();
        algorithmInfo->setText(pass::uiText(QStringLiteral("目前演算法：%1")).arg(algorithm->currentText()));
        ruleStatus->setText(error.isEmpty() ? (words && wordCount->value() < 6 ? pass::uiText(QStringLiteral("規則可用；EFF 建議至少使用 6 個單詞。")) : pass::uiText(QStringLiteral("規則可用；密碼以安全亂數均勻取樣。"))) : error);
        const bool dark=qApp->palette().color(QPalette::Window).lightness()<128;
        ruleStatus->setStyleSheet(error.isEmpty()?(dark?"color:#65cfb6":"color:#126c56"):(dark?"color:#f09191":"color:#a82929"));
        if(error.isEmpty() && !operationSource.isEmpty())ruleStatus->setText(pass::uiText(operationSource));
        generateButton->setEnabled(error.isEmpty() && !job && !generationBusy.load());
        // An abandoned worker may still hold the single generation slot; poll until it is released.
        if((job || generationBusy.load()) && !pollTimer.isActive())pollTimer.start();
        characterPage->setEnabled(!job); wordPage->setEnabled(!job && pass::wordlist().available());
        for (int i = 0; i < 4; ++i) minima[i]->setEnabled(enabled[i]->isChecked());
        if (words) {
            entropyLabel->setText(error.isEmpty() ? pass::uiText(QStringLiteral("產生規則熵：約 %1 bits（分隔符不增加熵）")).arg(pass::entropy(phraseRules()),0,'f',1) : pass::uiText(QStringLiteral("請先修正產生規則。")));
            inspector->setTitle(pass::uiText(QStringLiteral("單詞密碼格式檢查"))); strength->hide();
            analysisLabel->setText(pass::uiText(QStringLiteral("每個單詞獨立均勻抽樣，允許重複。手動輸入的片語不視為隨機產生。")));
            matchLabel->setText(check->text().isEmpty() ? QString() : pass::matches(check->text(),phraseRules()) ? pass::uiText(QStringLiteral("符合目前詞表、詞數與分隔格式")) : pass::uiText(QStringLiteral("不符合目前詞表、詞數或分隔格式")));
        } else {
            entropyLabel->setText(error.isEmpty()?pass::uiText(QStringLiteral("有效組合熵：約 %1 bits（依目前產生規則）")).arg(pass::entropy(rules()),0,'f',1):pass::uiText(QStringLiteral("請先修正產生規則。")));
            inspector->setTitle(pass::uiText(QStringLiteral("密碼強度檢查"))); strength->show();
            QString text = check->text(); const auto a=pass::analyze(text); wipeText(text);
            strength->setValue(a.score); strength->setFormat(pass::uiText(a.label)+"  %p%");
            analysisLabel->setText(pass::uiText(a.feedback.join(QStringLiteral("\n"))));
            matchLabel->setText(check->text().isEmpty()?QString():pass::matches(check->text(),rules())?pass::uiText(QStringLiteral("符合目前產生規則")):pass::uiText(QStringLiteral("不符合目前產生規則（長度／字元／最低數量）")));
        }
    }
    void saveRules() {
        settings.setValue("random/algorithm",algorithm->currentData().toString());
        settings.setValue("passphrase/words",wordCount->value());
        settings.setValue("passphrase/batch",wordBatch->value());
        settings.setValue("passphrase/separator",separator->currentData().toString());
        settings.setValue("length",length->value()); settings.setValue("batch",batch->value());
        for(int i=0;i<4;++i) {settings.setValue(QString("enabled%1").arg(i),enabled[i]->isChecked());settings.setValue(QString("minimum%1").arg(i),minima[i]->value());}
        settings.setValue("ambiguous",ambiguous->isChecked()); settings.setValue("autoClear",autoClear->isChecked());
        settings.setValue("ui/language",language->currentData().toString());
        settings.setValue("ui/appearance",appearance->currentData().toString());
        settings.remove("custom"); settings.remove("excluded");
        // Allowlist only: discard legacy secret/history/unknown keys.
        const QSet<QString> allowed={"ui/appearance","ui/language","random/algorithm","length","batch","ambiguous","autoClear","enabled0","enabled1","enabled2","enabled3","minimum0","minimum1","minimum2","minimum3","passphrase/words","passphrase/batch","passphrase/separator"};
        for(const auto& key:settings.allKeys())if(!allowed.contains(key))settings.remove(key);
        settings.sync();
        if(settings.status()!=QSettings::NoError)copyStatus->setText(pass::uiText(QStringLiteral("偏好保存失敗；本次選擇仍有效。")));
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
        pass::uiLanguage=0;
        setWindowTitle(titleText()); setWindowIcon(QIcon(":/app.ico"));
        const auto screen=QGuiApplication::primaryScreen();const QRect available=screen?screen->availableGeometry():QRect(0,0,1080,830);
        resize(qMin(1040,qMax(400,available.width()-32)),qMin(790,qMax(400,available.height()-32)));
        auto root=new QWidget; setCentralWidget(root); auto page=new QVBoxLayout(root); page->setContentsMargins(28,24,28,24); page->setSpacing(16);
        auto title=new QLabel(pass::uiText(QStringLiteral("為每個帳號，產生獨一無二的密碼"))); title->setObjectName("title"); page->addWidget(title);
        language=new QComboBox(this);language->setObjectName("uiLanguage");language->hide();
        language->addItem(QStringLiteral("繁體中文"),"zh-TW");language->addItem(QStringLiteral("日本語"),"ja");language->addItem(QStringLiteral("English"),"en");
        appearance=new QComboBox(this);appearance->setObjectName("uiAppearance");appearance->hide();
        appearance->addItem(QStringLiteral("跟隨 Windows"),"system");appearance->addItem(QStringLiteral("淺色"),"light");appearance->addItem(QStringLiteral("深色"),"dark");
        int theme=appearance->findData(settings.value("ui/appearance","system"));appearance->setCurrentIndex(theme<0?0:theme);
        auto columns=new QHBoxLayout; columns->setSpacing(20); page->addLayout(columns,1);
        auto left=new QGroupBox(pass::uiText(QStringLiteral("產生規則")));left->setObjectName("rulesCard"); auto form=new QVBoxLayout(left);form->setSpacing(12);columns->addWidget(left,1);
        algorithm = new QComboBox(this); algorithm->hide(); algorithm->setObjectName("randomAlgorithm");
        algorithm->addItem(QStringLiteral("CTR-DRBG · AES-256-CTR"), "CTR-DRBG");
        algorithm->addItem(QStringLiteral("HASH-DRBG · SHA-256"), "HASH-DRBG");
        algorithm->addItem(QStringLiteral("HMAC-DRBG · SHA-256"), "HMAC-DRBG");
        algorithm->addItem(QStringLiteral("HMAC-DRBG · SHA-512"), "HMAC-DRBG-SHA512");
        const QString savedAlgorithm = settings.value("random/algorithm", "CTR-DRBG").toString();
        int selected = algorithm->findData(savedAlgorithm);
        if (selected < 0) { algorithm->addItem(pass::uiText(QStringLiteral("無效設定，請選擇演算法")), savedAlgorithm); selected=4; }
        algorithm->setCurrentIndex(selected);
        algorithmInfo=new QLabel;algorithmInfo->setObjectName("algorithmInfo");algorithmInfo->setWordWrap(true);form->addWidget(algorithmInfo);
        tabs = new QTabWidget; tabs->setObjectName("modeTabs");tabs->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Ignored);tabs->setMinimumHeight(60);form->addWidget(tabs,1);
        characterPage = new QWidget;characterPage->setObjectName("settingsPage"); auto charForm = new QVBoxLayout(characterPage);
        charForm->setSpacing(12); tabs->addTab(scrollRules(characterPage),pass::uiText(QStringLiteral("隨機字元密碼")));
        auto numbers=new QFormLayout; length=new QSpinBox;length->setObjectName("characterLength");length->setRange(4,128);length->setValue(intSetting("length",20,4,128));batch=new QSpinBox;batch->setObjectName("characterBatch");batch->setRange(1,100);batch->setValue(intSetting("batch",1,1,100));numbers->addRow(pass::uiText(QStringLiteral("密碼長度")),length);numbers->addRow(pass::uiText(QStringLiteral("產生數量")),batch);charForm->addLayout(numbers);
        auto grid=new QGridLayout;grid->addWidget(new QLabel(pass::uiText(QStringLiteral("啟用字元類型"))),0,0);grid->addWidget(new QLabel(pass::uiText(QStringLiteral("最低數量"))),0,1);
        const QStringList names={pass::uiText(QStringLiteral("大寫字母  A–Z")),pass::uiText(QStringLiteral("小寫字母  a–z")),pass::uiText(QStringLiteral("數字  0–9")),pass::uiText(QStringLiteral("符號  !@#$…"))};
        for(int i=0;i<4;++i) { auto box=new QCheckBox(names[i]);box->setChecked(settings.value(QString("enabled%1").arg(i),true).toBool());enabled+=box;auto spin=new QSpinBox;spin->setObjectName(QString("minimum%1").arg(i));spin->setRange(0,8);spin->setValue(intSetting(QString("minimum%1").arg(i),1,0,8));spin->setEnabled(box->isChecked());minima+=spin;grid->addWidget(box,i+1,0);grid->addWidget(spin,i+1,1);connect(box,&QCheckBox::toggled,spin,&QWidget::setEnabled); }
        charForm->addLayout(grid);ambiguous=new QCheckBox(pass::uiText(QStringLiteral("排除易混淆字元  I l 1 O 0 o |")));ambiguous->setChecked(settings.value("ambiguous",true).toBool());charForm->addWidget(ambiguous);
        auto chars=new QFormLayout;custom=new QLineEdit;custom->setPlaceholderText(pass::uiText(QStringLiteral("加入字元池，例如：~<>")));excluded=new QLineEdit;excluded->setPlaceholderText(pass::uiText(QStringLiteral("不要出現的字元")));custom->setMaxLength(256);excluded->setMaxLength(256);chars->addRow(pass::uiText(QStringLiteral("自訂字元")),custom);chars->addRow(pass::uiText(QStringLiteral("排除字元")),excluded);charForm->addLayout(chars);
        auto note=new QLabel(pass::uiText(QStringLiteral("自訂字元會加入字元池；關閉全部類型即可只用自訂字元。限可見 ASCII；每類最低數量 0–8。")));note->setWordWrap(true);charForm->addWidget(note);
        charForm->addStretch();
        wordPage = new QWidget;wordPage->setObjectName("settingsPage"); auto wordForm = new QVBoxLayout(wordPage); wordForm->setSpacing(12);
        tabs->addTab(scrollRules(wordPage),pass::uiText(QStringLiteral("隨機單詞密碼")));
        auto source = new QLabel(pass::uiText(QStringLiteral("EFF Long Wordlist · 7,776 詞 · 離線"))); source->setWordWrap(true); wordForm->addWidget(source);
        auto wordNumbers = new QFormLayout;
        wordCount = new QSpinBox; wordCount->setObjectName("wordCount"); wordCount->setRange(4,12); wordCount->setValue(intSetting("passphrase/words",6,4,12));
        wordBatch = new QSpinBox; wordBatch->setObjectName("wordBatch"); wordBatch->setRange(1,100); wordBatch->setValue(intSetting("passphrase/batch",1,1,100));
        separator = new QComboBox; separator->setObjectName("separator");
        separator->addItem(pass::uiText(QStringLiteral("連字號  -")),QStringLiteral("-")); separator->addItem(pass::uiText(QStringLiteral("底線  _")),QStringLiteral("_")); separator->addItem(pass::uiText(QStringLiteral("空白")),QStringLiteral(" "));
        const int savedSeparator = separator->findData(settings.value("passphrase/separator",QStringLiteral("-")));
        separator->setCurrentIndex(savedSeparator < 0 ? 0 : savedSeparator);
        wordNumbers->addRow(pass::uiText(QStringLiteral("單詞數量")),wordCount); wordNumbers->addRow(pass::uiText(QStringLiteral("分隔符號")),separator); wordNumbers->addRow(pass::uiText(QStringLiteral("產生數量")),wordBatch); wordForm->addLayout(wordNumbers);
        auto wordNote = new QLabel(pass::uiText(QStringLiteral("預設 6 詞；保留官方英文小寫，允許重複單詞。請先確認網站接受的字元與長度，勿截斷產生結果。"))); wordNote->setWordWrap(true); wordForm->addWidget(wordNote);
        if (!pass::wordlist().available()) { auto failure = new QLabel(pass::uiText(pass::wordlist().error)); failure->setWordWrap(true); failure->setStyleSheet("color:#c54141"); wordForm->addWidget(failure); }
        wordForm->addStretch();
        ruleStatus=new QLabel;ruleStatus->setWordWrap(true);form->addWidget(ruleStatus);entropyLabel=new QLabel;entropyLabel->setWordWrap(true);form->addWidget(entropyLabel);form->addStretch();
        generateButton=new QPushButton(pass::uiText(QStringLiteral("產生密碼")));generateButton->setObjectName("primary");generateButton->setMinimumHeight(44);cancelButton=new QPushButton(pass::uiText(QStringLiteral("取消產生")));cancelButton->setEnabled(false);form->addWidget(generateButton);form->addWidget(cancelButton);
        auto right=new QWidget;auto rightLayout=new QVBoxLayout(right);rightLayout->setContentsMargins(0,0,0,0);columns->addWidget(scrollRules(right),1);
        auto results=new QGroupBox(pass::uiText(QStringLiteral("產生結果")));auto resultLayout=new QVBoxLayout(results);rightLayout->addWidget(results,1);
        output=new QPlainTextEdit;output->setReadOnly(true);output->setUndoRedoEnabled(false);output->setPlaceholderText(pass::uiText(QStringLiteral("按下「產生密碼」開始")));output->setFont(QFont("Consolas",12));output->setContextMenuPolicy(Qt::NoContextMenu);resultLayout->addWidget(output);
        lengthLabel = new QLabel; lengthLabel->setObjectName("resultLength"); resultLayout->addWidget(lengthLabel);
        visible=new QCheckBox(pass::uiText(QStringLiteral("顯示密碼")));visible->setChecked(false);resultLayout->addWidget(visible);autoClear=new QCheckBox(pass::uiText(QStringLiteral("複製後 30 秒清除剪貼簿")));autoClear->setChecked(settings.value("autoClear",true).toBool());resultLayout->addWidget(autoClear);
        auto buttons=new QHBoxLayout;auto copy=new QPushButton(pass::uiText(QStringLiteral("複製全部")));auto erase=new QPushButton(pass::uiText(QStringLiteral("清除結果")));buttons->addWidget(copy);buttons->addWidget(erase);resultLayout->addLayout(buttons);copyStatus=new QLabel;copyStatus->setWordWrap(true);resultLayout->addWidget(copyStatus);
        inspector=new QGroupBox(pass::uiText(QStringLiteral("密碼強度檢查")));auto inspectLayout=new QVBoxLayout(inspector);rightLayout->addWidget(inspector);
        check=new QLineEdit;check->setMaxLength(128);check->setEchoMode(QLineEdit::Password);check->setPlaceholderText(pass::uiText(QStringLiteral("輸入密碼，或使用第一筆產生結果")));inspectLayout->addWidget(check);strength=new QProgressBar;strength->setRange(0,100);inspectLayout->addWidget(strength);matchLabel=new QLabel;matchLabel->setWordWrap(true);inspectLayout->addWidget(matchLabel);analysisLabel=new QLabel;analysisLabel->setWordWrap(true);inspectLayout->addWidget(analysisLabel);
        clearTimer.setParent(this);clearTimer.setObjectName("clipboardTimer");clearTimer.setSingleShot(true);clearTimer.setTimerType(Qt::PreciseTimer);
        connect(&clearTimer,&QTimer::timeout,this,[this]{clearCopied();});
        connect(copy,&QPushButton::clicked,this,[this]{
            if(!generated)return;
            QString text=secrets.text();
            if(!copyToClipboard(text)){wipeText(text);copyStatus->setText(pass::uiText(QStringLiteral("無法複製；剪貼簿目前無法存取。")));return;}
            clipboardElapsed.start();clipboardRetryCount=0;clipboardWiped=false;
            copied.reserve(static_cast<size_t>(text.size()));copied.append(text);wipeText(text);
            clearTimer.stop();countdownTimer.stop();if(autoClear->isChecked()){clearTimer.start(30000);countdownTimer.start(200);}
            copyStatus->setText(autoClear->isChecked()?pass::uiText(QStringLiteral("已複製；30 秒後只清除仍相同的內容。")):pass::uiText(QStringLiteral("已複製。")));
        });
        connect(erase,&QPushButton::clicked,this,[this]{abandonJob();clearResults();clearCopied();refresh();});
        connect(visible,&QCheckBox::toggled,this,[this](bool show){check->setEchoMode(show?QLineEdit::Normal:QLineEdit::Password);if(generated)displayResults();});
#ifdef PASS_TESTING
        connect(generateButton,&QPushButton::clicked,this,[this,generator,wordGenerator]{
#else
        connect(generateButton,&QPushButton::clicked,this,[this]{
#endif
            operationSource.clear();saveRules();clearResults();
            try {
#ifdef PASS_TESTING
                job = wordMode() ? startGeneration(phraseRules(),wordBatch->value(),wordGenerator) : startGeneration(rules(),batch->value(),generator);
#else
                job = wordMode() ? startGeneration(phraseRules(),wordBatch->value()) : startGeneration(rules(),batch->value());
#endif
            }
            catch(const std::exception&) {showOperation(QStringLiteral("無法啟動背景工作。"));return;}
            if(!job){showOperation(QStringLiteral("安全亂數服務仍在處理先前工作，請稍後重試。"));return;}
            characterPage->setEnabled(false);wordPage->setEnabled(false);generateButton->setEnabled(false);cancelButton->setEnabled(true);showOperation(QStringLiteral("正在產生…"));pollTimer.start();
        });
        connect(cancelButton,&QPushButton::clicked,this,[this]{abandonJob();refresh();showOperation(QStringLiteral("已取消；不會顯示本次結果。"));});
        pollTimer.setInterval(25);
        connect(&pollTimer,&QTimer::timeout,this,[this]{
            if(!job){
                generateButton->setEnabled(!generationBusy.load() && validationError().isEmpty());
                // Poll only while an abandoned worker is still finishing; idle windows do no periodic work.
                if(!generationBusy.load())pollTimer.stop();
                return;
            }
            if(std::chrono::steady_clock::now()-job->started>=std::chrono::seconds(10)){
                abandonJob();refresh();showOperation(QStringLiteral("等待安全亂數服務逾時，已停止接收結果。"));return;
            }
            if(!job->done.load())return;
            auto completed=std::move(job);cancelButton->setEnabled(false);
            std::lock_guard<std::mutex> lock(completed->mutex);
            operationSource.clear();refresh();
            if(!completed->error.isEmpty()){showOperation(completed->error);return;}
            secrets.swap(completed->secret);generated=true;displayResults();
            QString text=secrets.text();QString first=text.section('\n',0,0);check->setText(first);wipeText(first);wipeText(text);copyStatus->clear();
        });
        connect(length,&QSpinBox::valueChanged,this,[this]{refresh();});for(auto spin:minima)connect(spin,&QSpinBox::valueChanged,this,[this]{refresh();});for(auto box:enabled)connect(box,&QCheckBox::toggled,this,[this]{refresh();});connect(ambiguous,&QCheckBox::toggled,this,[this]{refresh();});for(auto field:{custom,excluded,check})connect(field,&QLineEdit::textChanged,this,[this]{refresh();});
        connect(wordCount,&QSpinBox::valueChanged,this,[this]{refresh();});
        connect(separator,&QComboBox::currentIndexChanged,this,[this]{refresh();});
        connect(tabs,&QTabWidget::currentChanged,this,[this]{
            operationSource.clear();abandonJob(); clearResults(); clearCopied(); visible->setChecked(false);
            check->setMaxLength(wordMode() && pass::wordlist().available() ? 12 * pass::wordlist().maxLength + 11 : 128);
            refresh();
        });
        connect(algorithm,&QComboBox::currentIndexChanged,this,[this]{
            operationSource.clear();abandonJob(); clearResults(); clearCopied(); saveRules(); settings.sync(); refresh();
        });
        algorithmMenu=menuBar()->addMenu(QString());algorithmMenu->setObjectName("algorithmMenu");
        languageMenu=menuBar()->addMenu(QString());languageMenu->setObjectName("languageMenu");
        appearanceMenu=menuBar()->addMenu(QString());appearanceMenu->setObjectName("appearanceMenu");
        helpMenu=menuBar()->addMenu(QString());helpMenu->setObjectName("helpMenu");
        auto bindMenu=[this](QMenu* menu,QComboBox* combo){
            auto group=new QActionGroup(menu);group->setExclusive(true);
            for(int i=0;i<combo->count();++i){auto action=menu->addAction(combo->itemText(i));action->setCheckable(true);action->setData(i);group->addAction(action);action->setChecked(i==combo->currentIndex());
                connect(action,&QAction::triggered,this,[combo,i]{combo->setCurrentIndex(i);});}
            connect(combo,&QComboBox::currentIndexChanged,menu,[group](int index){for(auto action:group->actions())action->setChecked(action->data().toInt()==index);});
        };
        bindMenu(algorithmMenu,algorithm);bindMenu(languageMenu,language);bindMenu(appearanceMenu,appearance);
        aboutAction=helpMenu->addAction(QString());aboutAction->setObjectName("aboutAction");
        connect(aboutAction,&QAction::triggered,this,[this]{QMessageBox::about(this,pass::uiText(QStringLiteral("關於 Pass-SSL")),aboutText());});
        aboutQtAction=helpMenu->addAction(QString());aboutQtAction->setObjectName("aboutQtAction");
        connect(aboutQtAction,&QAction::triggered,this,[this]{QMessageBox::aboutQt(this,pass::uiText(QStringLiteral("關於 Qt")));});
        connect(appearance,&QComboBox::currentIndexChanged,this,[this]{applyAppearance(*qApp,appearance->currentData().toString());refresh();saveRules();});
        connect(qApp->styleHints(),&QStyleHints::colorSchemeChanged,this,[this]{if(appearance->currentData()=="system"){applyAppearance(*qApp,"system");refresh();}});
        applyAppearance(*qApp,appearance->currentData().toString());
        // Capture only public captions, never editable fields or results.
        for(auto widget:findChildren<QWidget*>()) {
            if(auto label=qobject_cast<QLabel*>(widget))label->setProperty("caption",label->text());
            if(auto box=qobject_cast<QGroupBox*>(widget))box->setProperty("caption",box->title());
            if(auto button=qobject_cast<QAbstractButton*>(widget))button->setProperty("caption",button->text());
            if(auto field=qobject_cast<QLineEdit*>(widget))field->setProperty("placeholder",field->placeholderText());
            if(auto field=qobject_cast<QPlainTextEdit*>(widget))field->setProperty("placeholder",field->placeholderText());
        }
        for(int i=0;i<tabs->count();++i)tabs->widget(i)->setProperty("tabCaption",tabs->tabText(i));
        for(auto combo:{separator,algorithm,appearance})for(int i=0;i<combo->count();++i)combo->setItemData(i,combo->itemText(i),Qt::UserRole+1);
        connect(language,&QComboBox::currentIndexChanged,this,[this]{retranslate();saveRules();});
        const int lang=language->findData(settings.value("ui/language","zh-TW"));
        language->setCurrentIndex(lang<0?0:lang);retranslate();
        countdownTimer.setParent(this);countdownTimer.setObjectName("clipboardCountdown");
        connect(&countdownTimer,&QTimer::timeout,this,[this]{updateCountdown();});
        connect(QApplication::clipboard(),&QClipboard::dataChanged,this,[this]{
            if(!writingClipboard && !copied.empty() && GetClipboardSequenceNumber()!=clipboardSequence){
                clearTimer.stop();countdownTimer.stop();copied.clear();clipboardSequence=0;
                copyStatus->setText(pass::uiText(QStringLiteral("剪貼簿已由其他複製取代。")));
            }
        });
        connect(autoClear,&QCheckBox::toggled,this,[this](bool on){
            if(!on){clearTimer.stop();countdownTimer.stop();}
            else if(!copied.empty()){clipboardRetryCount=0;clipboardElapsed.start();clearTimer.start(30000);countdownTimer.start(200);updateCountdown();}
        });
        saveRules();refresh();
        // Force the native HWND now; WinIdChange applies protection before show().
        (void)winId();
    }
    static QString titleText() {
        return QStringLiteral("Pass-SSL v%1 · %2").arg(QStringLiteral(PASS_VERSION_STRING),pass::uiText(QStringLiteral("安全密碼產生器")));
    }
    QString aboutText() const {
        return QStringLiteral("Pass-SSL v%1\n%2\nQt %3\n\n%4\n%5\n\n%6\n%7")
            .arg(QStringLiteral(PASS_VERSION_STRING),QString::fromLatin1(OpenSSL_version(OPENSSL_VERSION)),QString::fromLatin1(qVersion()),
                 pass::uiText(QStringLiteral("作者：%1")).arg(QStringLiteral(PASS_AUTHOR)),QStringLiteral(PASS_COPYRIGHT " · MIT License"),
                 pass::uiText(QStringLiteral("第三方元件：Qt（LGPLv3）、OpenSSL（Apache License 2.0）、zxcvbn-c（MIT）、EFF Long Wordlist（CC BY 4.0）。")),
                 pass::uiText(QStringLiteral("完整授權條款見程式目錄的 LICENSE.txt、THIRD_PARTY_NOTICES.md 與 LICENSES 資料夾。")));
    }
    void updateCountdown(){
        if(!copied.empty() && GetClipboardSequenceNumber()!=clipboardSequence){
            clearTimer.stop();countdownTimer.stop();copied.clear();clipboardSequence=0;
            copyStatus->setText(pass::uiText(QStringLiteral("剪貼簿已由其他複製取代。")));return;
        }
        if(!copied.empty() && clearTimer.isActive())copyStatus->setText(pass::uiText(QStringLiteral("已複製；%1 秒後安全清除。")).arg(qMax<qint64>(0,(30000-clipboardElapsed.elapsed()+999)/1000)));
    }
    static void excludeFromClipboardHistory(){
        // Ask Windows not to keep this item in Win+V history or sync it to other devices.
        // https://learn.microsoft.com/windows/win32/dataxchg/clipboard-formats
        static const UINT exclude=RegisterClipboardFormatW(L"ExcludeClipboardContentFromMonitorProcessing");
        static const UINT history=RegisterClipboardFormatW(L"CanIncludeInClipboardHistory");
        static const UINT cloud=RegisterClipboardFormatW(L"CanUploadToCloudClipboard");
        for(const UINT format:{exclude,history,cloud}){
            if(!format)continue;
            HGLOBAL value=GlobalAlloc(GMEM_MOVEABLE,sizeof(DWORD));if(!value)continue;
            if(auto data=static_cast<DWORD*>(GlobalLock(value))){*data=0;GlobalUnlock(value);if(SetClipboardData(format,value))continue;}
            GlobalFree(value);
        }
    }
    bool copyToClipboard(const QString& text){
        // Flush any previous Qt/OLE selection before taking native ownership.
        // Otherwise its delayed render callback can restore the previous copy.
        if(QApplication::clipboard()->ownsClipboard() && FAILED(OleFlushClipboard()))return false;
        const SIZE_T bytes=(static_cast<SIZE_T>(text.size())+1)*sizeof(wchar_t);
        HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,bytes);if(!memory)return false;
        auto clipboardData=static_cast<wchar_t*>(GlobalLock(memory));
        if(!clipboardData){GlobalFree(memory);return false;}
        std::memcpy(clipboardData,text.utf16(),bytes-sizeof(wchar_t));clipboardData[text.size()]=0;GlobalUnlock(memory);
        writingClipboard=true;bool transferred=false;
        if(OpenClipboard(reinterpret_cast<HWND>(winId()))){
            if(EmptyClipboard()){excludeFromClipboardHistory();if(SetClipboardData(CF_UNICODETEXT,memory))transferred=true;}
            CloseClipboard();
        }
        // Windows adds synthesized formats on CloseClipboard, advancing the sequence.
        clipboardSequence=transferred?GetClipboardSequenceNumber():0;
        if(transferred && GetClipboardOwner()!=reinterpret_cast<HWND>(winId()))clipboardSequence=0;
        writingClipboard=false;
        if(!transferred){clipboardData=static_cast<wchar_t*>(GlobalLock(memory));if(clipboardData){SecureZeroMemory(clipboardData,bytes);GlobalUnlock(memory);}GlobalFree(memory);return false;}
        return clipboardSequence!=0;
    }
    void retranslate(){
        pass::uiLanguage=language->currentData()=="ja"?2:language->currentData()=="en"?1:0;
        setFont(QFont(pass::uiLanguage==2?QStringLiteral("Yu Gothic UI"):pass::uiLanguage==1?QStringLiteral("Segoe UI"):QStringLiteral("Microsoft JhengHei UI"),10));
        qApp->removeTranslator(&qtTranslator);
        if(pass::uiLanguage!=1){
            const QString file=pass::uiLanguage==2?QStringLiteral(":/i18n/qtbase_ja.qm"):QStringLiteral(":/i18n/qtbase_zh_TW.qm");
            if(qtTranslator.load(file))qApp->installTranslator(&qtTranslator);
        }
        setWindowTitle(titleText());
        for(auto widget:findChildren<QWidget*>()) {
            const auto caption=widget->property("caption");
            if(caption.isValid()){
                const auto value=pass::uiText(caption.toString());
                if(auto label=qobject_cast<QLabel*>(widget))label->setText(value);
                if(auto box=qobject_cast<QGroupBox*>(widget))box->setTitle(value);
                if(auto button=qobject_cast<QAbstractButton*>(widget))button->setText(value);
            }
            const auto placeholder=widget->property("placeholder");
            if(placeholder.isValid()){
                if(auto field=qobject_cast<QLineEdit*>(widget))field->setPlaceholderText(pass::uiText(placeholder.toString()));
                if(auto field=qobject_cast<QPlainTextEdit*>(widget))field->setPlaceholderText(pass::uiText(placeholder.toString()));
            }
        }
        for(int i=0;i<tabs->count();++i)tabs->setTabText(i,pass::uiText(tabs->widget(i)->property("tabCaption").toString()));
        for(auto combo:{separator,algorithm,appearance})for(int i=0;i<combo->count();++i)combo->setItemText(i,pass::uiText(combo->itemData(i,Qt::UserRole+1).toString()));
        algorithmMenu->setTitle(pass::uiText(QStringLiteral("演算法")));
        languageMenu->setTitle(pass::uiText(QStringLiteral("語言")));
        appearanceMenu->setTitle(pass::uiText(QStringLiteral("外觀")));
        helpMenu->setTitle(pass::uiText(QStringLiteral("說明")));
        aboutAction->setText(pass::uiText(QStringLiteral("關於 Pass-SSL")));
        aboutQtAction->setText(pass::uiText(QStringLiteral("關於 Qt")));
        for(int i=0;i<appearance->count();++i)appearanceMenu->actions()[i]->setText(appearance->itemText(i));
        for(int i=0;i<algorithm->count();++i)algorithmMenu->actions()[i]->setText(algorithm->itemText(i));
        refresh();if(job)ruleStatus->setText(pass::uiText(QStringLiteral("正在產生…")));
        if(generated)displayResults();updateCountdown();
    }
    static QString masked(const QString& text) {QString value=text;for(QChar& c:value)if(c!='\n')c=QChar(0x2022);return value;}
    void abandonJob() { if(job){job->abandon();job.reset();} cancelButton->setEnabled(false); }
    void clearCopied(bool allowRetry=true) {
        clearTimer.stop();countdownTimer.stop();
        if(!clipboardSequence || copied.empty())return;
        if(allowRetry && clipboardRetryCount>0 && clipboardRetryElapsed.elapsed()>=5000){
            clipboardSequence=0;clipboardWiped=false;clipboardRetryCount=0;copied.clear();copyStatus->clear();return;
        }
        // Check again *under the clipboard lock*. External copies, including
        // identical text from the same app, must invalidate the retry session.
        bool retry=false;
        if(!OpenClipboard(reinterpret_cast<HWND>(winId())))retry=true;
        else {
            if(GetClipboardSequenceNumber()==clipboardSequence && GetClipboardOwner()==reinterpret_cast<HWND>(winId())){
                QString expected=copied.text();CandidateWiper wipe(expected);
                HANDLE handle=GetClipboardData(CF_UNICODETEXT);
                const SIZE_T bytes=handle?GlobalSize(handle):0;
                auto clipboardData=handle?static_cast<wchar_t*>(GlobalLock(handle)):nullptr;
                bool same=clipboardData && bytes>=(static_cast<SIZE_T>(expected.size())+1)*sizeof(wchar_t) && clipboardData[expected.size()]==0;
                if(same){
                    if(clipboardWiped){for(int i=0;i<expected.size();++i)if(clipboardData[i]!=0){same=false;break;}}
                    else same=std::memcmp(clipboardData,expected.utf16(),static_cast<size_t>(expected.size())*sizeof(wchar_t))==0;
                }
                if(same){SecureZeroMemory(clipboardData,(static_cast<SIZE_T>(expected.size())+1)*sizeof(wchar_t));clipboardWiped=true;}
                if(clipboardData)GlobalUnlock(handle);
                if(same){
                    writingClipboard=true;const bool cleared=clearClipboardContents()!=0;writingClipboard=false;
                    if(cleared)copyStatus->setText(pass::uiText(QStringLiteral("剪貼簿已安全清除。")));
                    else retry=true;
                } else if(!clipboardData)retry=true;
            }
            CloseClipboard();
        }
        // Non-blocking bounded retries: at most 50 retries, at 100ms intervals,
        // and no retry more than 5 seconds after the first failed attempt.
        if(retry && allowRetry){
            if(clipboardRetryCount==0)clipboardRetryElapsed.start();
            if(clipboardRetryCount<50 && clipboardRetryElapsed.elapsed()<5000){
                ++clipboardRetryCount;clearTimer.start(100);return;
            }
        }
        // Never report success after exhaustion or shutdown failure.
        if(retry)copyStatus->clear();
        clipboardSequence=0;clipboardWiped=false;clipboardRetryCount=0;copied.clear();
    }
    void clearResults() {
        generated=false;output->clear();check->clear();secrets.clear();copyStatus->clear();lengthLabel->clear();
    }
    void displayResults() {
        QString text=secrets.text();QString display=visible->isChecked()?QString(text.constData(),text.size()):masked(text);
        output->setPlainText(display);
        const qsizetype firstLength = text.indexOf('\n') < 0 ? text.size() : text.indexOf('\n');
        lengthLabel->setText(pass::uiText(QStringLiteral("第一筆長度：%1 字元")).arg(firstLength));
        wipeText(display);wipeText(text);
    }
    ~Window() override {pollTimer.stop();abandonJob();saveRules();clearResults();clearCopied(false);}
};
void applyAppearance(QApplication& app, const QString& choice) {
    // Leave system detection enabled even when the user explicitly chooses a theme.
    const bool dark=choice=="dark" || (choice=="system" && app.styleHints()->colorScheme()==Qt::ColorScheme::Dark);
    app.setStyle("Fusion");
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
    QApplication app(argc,argv);QCoreApplication::setOrganizationName("PassLocal");QCoreApplication::setApplicationName("Pass-SSL");QCoreApplication::setApplicationVersion(QStringLiteral(PASS_VERSION_STRING));
    // Validation never changes the user's real preferences.
    QTemporaryDir validationSettings;
    if(app.arguments().contains("--smoke-test") || app.arguments().contains("--screenshot")){
        if(!validationSettings.isValid())return 6;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,validationSettings.path());
    }
    applyAppearance(app);
    Window window;window.show();
    if(app.arguments().contains("--word-tab"))window.findChild<QTabWidget*>("modeTabs")->setCurrentIndex(1);
    const QStringList args=app.arguments();if(args.contains("--smoke-test")){if(!pass::wordlist().available())return 2;
        try { for(auto a:{pass::RandomAlgorithm::Ctr,pass::RandomAlgorithm::Hash,pass::RandomAlgorithm::Hmac,pass::RandomAlgorithm::Hmac512}) {
            pass::Rules r;r.algorithm=a;auto text=pass::generate(r);if(!pass::matches(text,r))return 3;wipeText(text);
            pass::PassphraseRules w;w.algorithm=a;text=pass::generate(w);if(!pass::matches(text,w))return 4;wipeText(text);
        }} catch(...) {return 5;}
        int hold=250;const int option=args.indexOf("--smoke-hold-ms");
        if(option>=0 && option+1<args.size()){bool valid=false;const int value=args[option+1].toInt(&valid);if(!valid || value<250 || value>10000)return 7;hold=value;}
        QTimer::singleShot(hold,&app,&QApplication::quit);}
    int screenshot=args.indexOf("--screenshot");if(screenshot>=0 && screenshot+1<args.size())QTimer::singleShot(500,&app,[&]{window.grab().save(args[screenshot+1]);app.quit();});
    const int code=app.exec();
    // Cancel any detached worker and give it a bounded chance to finish before
    // static objects and OpenSSL are torn down. Never block exit indefinitely.
    window.abandonJob();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(generationBusy.load() && std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return code;
}
#endif
