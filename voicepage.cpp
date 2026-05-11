#include "voicepage.h"
#include "voiceagentcore.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QFile>
#include <QSettings>
#include <QInputDialog>
#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QDialog>
#include <QFileInfo>
#include <QDesktopServices>

VoicePage::VoicePage(VoiceAgentCore *agent, QWidget *parent) : QWidget(parent), m_voiceAgent(agent) {
    networkManager = new QNetworkAccessManager(this);

    //全局样式表
    this->setStyleSheet(R"(
        QWidget { font-family: "Segoe UI", "Microsoft YaHei", sans-serif; font-size: 13px; }
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QTextEdit { border: 1px solid #dcdfe6; border-radius: 6px; padding: 5px 8px; background-color: #ffffff; color: #333333; }
        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus, QTextEdit:focus { border: 1.5px solid #0060c0; background-color: #fcfdfe; }
        QPushButton { background-color: #ffffff; border: 1px solid #dcdfe6; border-radius: 6px; padding: 6px 16px; color: #333333; font-weight: 500; }
        QPushButton:hover { background-color: #f5f7fa; border-color: #c0c4cc; color: #0060c0; }
    )");

    setupUI();
    initAudio();

    refreshPresetList();
}

void VoicePage::applyCardShadow(QWidget *widget) {
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 20));
    shadow->setOffset(0, 4);
    widget->setGraphicsEffect(shadow);
}

void VoicePage::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(15, 15, 15, 15);

    // =====================================================
    // 1. 独立预设管理区
    // =====================================================
    QWidget *presetContainer = new QWidget();
    QHBoxLayout *presetLayout = new QHBoxLayout(presetContainer);
    presetLayout->setContentsMargins(0, 0, 0, 0);
    presetLayout->setSpacing(10);

    presetCombo = new QComboBox();
    presetCombo->setMinimumHeight(30);
    presetCombo->setMinimumWidth(160);
    presetCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    addPresetBtn = new QPushButton("+ 新建");
    addPresetBtn->setMinimumHeight(30);
    renamePresetBtn = new QPushButton("重命名");
    renamePresetBtn->setMinimumHeight(30);
    deletePresetBtn = new QPushButton("删除");
    deletePresetBtn->setMinimumHeight(30);

    savePresetBtn = new QPushButton("💾 保存当前参数");
    savePresetBtn->setMinimumHeight(30);
    savePresetBtn->setStyleSheet("background-color: #e3f2fd; color: #0277bd; font-weight: bold; border: 1px solid #81d4fa; border-radius: 4px; padding: 4px 8px;");

    presetLayout->addWidget(new QLabel("<b>🎛️ 快速预设:</b>"));
    presetLayout->addWidget(presetCombo);
    presetLayout->addWidget(addPresetBtn);
    presetLayout->addWidget(renamePresetBtn);
    presetLayout->addWidget(deletePresetBtn);
    presetLayout->addStretch();
    presetLayout->addWidget(savePresetBtn);


    // =====================================================
    // 2. 参数配置区 (页签)
    // =====================================================
    configTabs = new QTabWidget();
    applyCardShadow(configTabs);
    configTabs->setStyleSheet("QTabWidget::pane { border: 1px solid #ddd; background: #ffffff; border-radius: 4px; } "
                              "QTabBar::tab { padding: 8px 16px; border: 1px solid transparent; font-weight: bold; } "
                              "QTabBar::tab:selected { border-bottom: 2px solid #0078d4; color: #0078d4; }");

    // --- Tab 1: 语音设置 ---
    QWidget *tabCore = new QWidget();
    QVBoxLayout *coreLayout = new QVBoxLayout(tabCore);
    coreLayout->setContentsMargins(10, 10, 15, 10);

    coreLayout->addLayout(createSliderWithSpinBox("语速:", 0.5, 2.0, 0.1, 1.0, speedSlider, speedSpin, 1));
    coreLayout->addLayout(createSliderWithSpinBox("音调:", -10.0, 10.0, 1.0, 0.0, pitchSlider, pitchSpin, 1));
    coreLayout->addLayout(createSliderWithSpinBox("句间停顿(秒):", 0.0, 5.0, 0.1, 0.5, intervalSlider, intervalSpin, 2));

    QFrame *line1 = new QFrame(); line1->setFrameShape(QFrame::HLine); line1->setStyleSheet("background-color: #e0e0e0; margin: 5px 0px;");
    coreLayout->addWidget(line1);

    coreLayout->addWidget(new QLabel("<span style='color:#666; font-size:14px;'>💡 进阶参数：这部分建议直接使用默认的，有什么问题再回来调！</span>"));
    coreLayout->addLayout(createSliderWithSpinBox("语调起伏 (Temp):", 0.1, 1.5, 0.05, 0.7, tempSlider, tempSpin));
    coreLayout->addLayout(createSliderWithSpinBox("平稳度 (Top-P):", 0.1, 1.0, 0.05, 0.8, topPSlider, topPSpin));
    coreLayout->addLayout(createIntSliderWithSpinBox("多变性 (Top-K):", 1, 100, 50, topKSlider, topKSpin));
    coreLayout->addStretch();

    QScrollArea *coreScroll = new QScrollArea();
    coreScroll->setWidgetResizable(true);
    coreScroll->setFrameShape(QFrame::NoFrame);
    coreScroll->setWidget(tabCore);
    configTabs->addTab(coreScroll, "⚙️ 语音设置");

    // --- Tab 2: 更多设置 ---
    QWidget *tabEnhance = new QWidget();
    QVBoxLayout *enhanceLayout = new QVBoxLayout(tabEnhance);
    enhanceLayout->setContentsMargins(10, 10, 15, 10);

    QHBoxLayout *splitLayout = new QHBoxLayout();
    splitCombo = new QComboBox();
    splitCombo->addItems({"按标点符号切分（注：可以都试试，哪个切分效果好就选哪个）", "四句一切", "按最大字数强制切分", "不切分"});
    splitLayout->addWidget(new QLabel("长段落切分策略:"));
    splitLayout->addWidget(splitCombo, 1);
    enhanceLayout->addLayout(splitLayout);

    QHBoxLayout *batchLayout = new QHBoxLayout();
    batchSpin = new QSpinBox();
    batchSpin->setRange(1, 16);
    batchSpin->setValue(2);
    batchSpin->setMinimumWidth(80);
    batchLayout->addWidget(new QLabel("生成并行量 (Batch Size):"));
    batchLayout->addWidget(batchSpin, 1);
    enhanceLayout->addLayout(batchLayout);

    QFrame *line2 = new QFrame(); line2->setFrameShape(QFrame::HLine); line2->setStyleSheet("background-color: #e0e0e0; margin: 5px 0px;");
    enhanceLayout->addWidget(line2);

    QHBoxLayout *llmLayout = new QHBoxLayout();
    useLlmEmotionCheck = new QCheckBox("启用 LLM 前置分析文本情感");
    llmModelCombo = new QComboBox();
    llmModelCombo->addItems({"gpt-4o-mini", "Qwen-Max", "Claude-3-Haiku"});
    llmModelCombo->setEnabled(false);
    connect(useLlmEmotionCheck, &QCheckBox::toggled, llmModelCombo, &QComboBox::setEnabled);
    llmLayout->addWidget(useLlmEmotionCheck);
    llmLayout->addWidget(new QLabel("模型:"));
    llmLayout->addWidget(llmModelCombo, 1);
    enhanceLayout->addLayout(llmLayout);

    cacheGroup = new QGroupBox("启用本地音频缓存 (参数相同时复用)");
    cacheGroup->setCheckable(true);
    cacheGroup->setChecked(true);
    //增加 margin-top 和 padding-top，去除导致裁切的负数
    cacheGroup->setStyleSheet("QGroupBox { border: 1px solid #e0e0e0; border-radius: 4px; margin-top: 20px; padding-top: 10px; } "
                              "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 10px; padding: 0 5px; }");
    QGridLayout *cacheLayout = new QGridLayout(cacheGroup);
    cacheLayout->setContentsMargins(15, 15, 15, 15);

    cacheExpireSpin = new QSpinBox();
    cacheExpireSpin->setRange(0, 720);
    cacheExpireSpin->setValue(24);
    cacheExpireSpin->setSuffix(" 小时 (0为永不过期)");
    cacheExpireSpin->setMinimumWidth(150);

    cachePathEdit = new QLineEdit(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/TTSCache");
    QPushButton *browseCacheBtn = new QPushButton("...");
    browseCacheBtn->setFixedWidth(30);
    connect(browseCacheBtn, &QPushButton::clicked, this, &VoicePage::onBrowseCachePath);

    cacheLayout->addWidget(new QLabel("有效期:"), 0, 0);
    cacheLayout->addWidget(cacheExpireSpin, 0, 1, 1, 2);
    cacheLayout->addWidget(new QLabel("路径:"), 1, 0);
    cacheLayout->addWidget(cachePathEdit, 1, 1);
    cacheLayout->addWidget(browseCacheBtn, 1, 2);
    enhanceLayout->addWidget(cacheGroup);

    enhanceLayout->addStretch();

    QScrollArea *enhanceScroll = new QScrollArea();
    enhanceScroll->setWidgetResizable(true);
    enhanceScroll->setFrameShape(QFrame::NoFrame);
    enhanceScroll->setWidget(tabEnhance);
    configTabs->addTab(enhanceScroll, "🛠️ 更多设置");


    // =====================================================
    // 3. 音频模型与参考配置区
    // =====================================================
    modelTabs = new QTabWidget();
    applyCardShadow(modelTabs);
    modelTabs->setStyleSheet("QTabWidget::pane { border: none; background: #ffffff; border-radius: 10px; } "
                             "QTabBar::tab { padding: 8px 16px; margin-right: 2px; font-weight: bold; color: #606266; background: transparent; } "
                             "QTabBar::tab:selected { color: #0078d4; border-bottom: 2px solid #0078d4; } "
                             "QTabBar::tab:hover:!selected { color: #333; border-bottom: 2px solid #ddd; }");

    // --- Tab 3.1: 快速声音克隆 ---
    QWidget *tabClone = new QWidget();
    QVBoxLayout *cloneLayout = new QVBoxLayout(tabClone);
    cloneLayout->setContentsMargins(20, 20, 20, 20);
    cloneLayout->setSpacing(12);

    QHBoxLayout *audioLayout = new QHBoxLayout();
    refAudioPathEdit = new QLineEdit();
    refAudioPathEdit->setPlaceholderText("选择一个3~10 秒的清晰参考音频 (.wav / .mp3)");
    refAudioPathEdit->setMinimumHeight(32);
    QPushButton *btnRefAudio = new QPushButton("浏览音频...");
    btnRefAudio->setMinimumHeight(32);
    audioLayout->addWidget(new QLabel("参考音频:"));
    audioLayout->addWidget(refAudioPathEdit, 1);
    audioLayout->addWidget(btnRefAudio);
    cloneLayout->addLayout(audioLayout);

    QHBoxLayout *refTextLayout = new QHBoxLayout();
    refTextEdit = new QLineEdit();
    refTextEdit->setPlaceholderText("输入参考音频中说的内容...");
    refTextEdit->setMinimumHeight(32);
    refLangCombo = new QComboBox();
    refLangCombo->addItems({"中文", "English", "日本語", "한국어"});
    refLangCombo->setFixedSize(100, 32);
    refTextLayout->addWidget(new QLabel("参考文本:"));
    refTextLayout->addWidget(refTextEdit, 1);
    refTextLayout->addWidget(new QLabel("语种:"));
    refTextLayout->addWidget(refLangCombo);
    cloneLayout->addLayout(refTextLayout);

    cloneLayout->addStretch();
    modelTabs->addTab(tabClone, "🎤 参考音频");

    // --- Tab 3.2: 本地环境训练 (配置向导入口) ---
    QWidget *tabTrain = new QWidget();
    QVBoxLayout *trainLayout = new QVBoxLayout(tabTrain);
    trainLayout->setContentsMargins(20, 15, 20, 15);
    trainLayout->setSpacing(8);

    QHBoxLayout *trainTopLayout = new QHBoxLayout();
    QPushButton *configTrainBtn = new QPushButton("⚙️ 开始训练...");
    configTrainBtn->setStyleSheet("background-color: #f0f8ff; color: #0277bd; font-weight: bold; border: 1px solid #81d4fa; padding: 6px 15px; border-radius: 4px;");
    configTrainBtn->setCursor(Qt::PointingHandCursor);

    trainTopLayout->addWidget(new QLabel("<b>训练日志:</b>"));
    trainTopLayout->addStretch();
    trainTopLayout->addWidget(configTrainBtn);
    trainLayout->addLayout(trainTopLayout);

    trainLogConsole = new QTextEdit();
    trainLogConsole->setReadOnly(true);
    trainLogConsole->setStyleSheet("background-color: #1e1e1e; color: #a6e22e; font-family: 'Consolas', monospace; font-size: 12px; border-radius: 6px; padding: 8px;");
    trainLogConsole->setPlaceholderText(">> 准备就绪。\n>> 请点击右上角【开始训练...】按钮打开自动化流水线。\n>> 后台 UVR5提取、切片、ASR打标 以及 训练进度 将显示在此处...");
    trainLayout->addWidget(trainLogConsole, 1);

    modelTabs->addTab(tabTrain, "🧪 模型训练（没有模型来这里👇）");

    // --- Tab 3.3: 高级模型导入 ---
    QWidget *tabAdvanced = new QWidget();
    QGridLayout *advLayout = new QGridLayout(tabAdvanced);
    advLayout->setContentsMargins(20, 20, 20, 20);
    advLayout->setSpacing(12);

    ckptPathEdit = new QLineEdit();
    ckptPathEdit->setPlaceholderText("导入 GPT 模型文件路径 (.ckpt)");
    ckptPathEdit->setMinimumHeight(32);
    QPushButton *btnCkpt = new QPushButton("浏览...");
    btnCkpt->setMinimumHeight(32);
    advLayout->addWidget(new QLabel("GPT 模型:"), 0, 0);
    advLayout->addWidget(ckptPathEdit, 0, 1);
    advLayout->addWidget(btnCkpt, 0, 2);

    pthPathEdit = new QLineEdit();
    pthPathEdit->setPlaceholderText("导入 SoVITS 模型文件路径 (.pth)");
    pthPathEdit->setMinimumHeight(32);
    QPushButton *btnPth = new QPushButton("浏览...");
    btnPth->setMinimumHeight(32);
    advLayout->addWidget(new QLabel("VITS 模型:"), 1, 0);
    advLayout->addWidget(pthPathEdit, 1, 1);
    advLayout->addWidget(btnPth, 1, 2);

    QLabel *advTip = new QLabel("<span style='color:#909399; font-size:12px;'>💡 提示：在此导入你从网上下载或自己训练好的专属模型。留空则使用默认基座。</span>");
    advLayout->addWidget(advTip, 2, 0, 1, 3);
    advLayout->setRowStretch(3, 1);

    modelTabs->addTab(tabAdvanced, "📁 模型导入");

    // --- 绑定路径浏览信号 ---
    auto browseFile = [this](QLineEdit* edit, const QString &filter) {
        QString path = QFileDialog::getOpenFileName(this, "选择文件", "", filter);
        if(!path.isEmpty()) edit->setText(path);
    };

    connect(btnRefAudio, &QPushButton::clicked, [=](){ browseFile(refAudioPathEdit, "Audio Files (*.wav *.mp3 *.flac)"); });
    connect(btnCkpt, &QPushButton::clicked, [=](){ browseFile(ckptPathEdit, "Checkpoint Files (*.ckpt)"); });
    connect(btnPth, &QPushButton::clicked, [=](){ browseFile(pthPathEdit, "VITS Files (*.pth)"); });

    // 打开自动化训练配置弹窗
    connect(configTrainBtn, &QPushButton::clicked, this, &VoicePage::openTrainingConfigDialog);


    // =====================================================
    // 4. 输入文本与操作区
    // =====================================================
    QGroupBox *inputGroup = new QGroupBox("📝 待合成正文");
    // [修复点2]：为主界面的输入框组同样预留充足的顶边距
    inputGroup->setStyleSheet("QGroupBox { font-weight: bold; border: 1px solid #ccc; border-radius: 6px; margin-top: 20px; padding-top: 10px; } "
                              "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }");
    QVBoxLayout *inputVBox = new QVBoxLayout(inputGroup);
    inputVBox->setContentsMargins(15, 15, 15, 15);

    inputText = new QTextEdit();
    inputText->setPlaceholderText("在这里输入你想要 AI 说的话...");
    inputVBox->addWidget(inputText);

    QHBoxLayout *btnBar = new QHBoxLayout();
    generateBtn = new QPushButton("✨ 开始合成");
    generateBtn->setFixedHeight(40);
    generateBtn->setStyleSheet("background-color: #0078d4; color: white; font-weight: bold; border-radius: 4px;");

    playBtn = new QPushButton("▶ 播放");
    playBtn->setEnabled(false);
    playBtn->setFixedWidth(100);

    exportFormatCombo = new QComboBox();
    exportFormatCombo->addItems({".wav", ".mp3"});
    exportFormatCombo->setFixedWidth(90);

    exportBtn = new QPushButton("💾 导出");
    exportBtn->setEnabled(false);
    exportBtn->setFixedWidth(100);

    btnBar->addWidget(generateBtn, 1);
    btnBar->addWidget(playBtn);
    btnBar->addWidget(exportFormatCombo);
    btnBar->addWidget(exportBtn);

    progressBar = new QProgressBar();
    progressBar->setVisible(false);
    progressBar->setTextVisible(false);
    progressBar->setFixedHeight(4);

    inputVBox->addLayout(btnBar);
    inputVBox->addWidget(progressBar);


    // =====================================================
    // 布局组装
    // =====================================================
    mainLayout->addWidget(presetContainer, 0);
    mainLayout->addWidget(configTabs, 5);
    mainLayout->addWidget(modelTabs, 2);
    mainLayout->addWidget(inputGroup, 3);

    // --- 核心操作信号连接 ---
    connect(generateBtn, &QPushButton::clicked, this, &VoicePage::onGenerateClicked);
    connect(playBtn, &QPushButton::clicked, this, &VoicePage::onPlayClicked);
    connect(exportBtn, &QPushButton::clicked, this, &VoicePage::onExportClicked);

    // --- 预设管理信号连接 ---
    connect(presetCombo, &QComboBox::currentTextChanged, this, &VoicePage::loadSelectedPreset);
    connect(addPresetBtn, &QPushButton::clicked, this, &VoicePage::addNewPreset);
    connect(renamePresetBtn, &QPushButton::clicked, this, &VoicePage::renamePreset);
    connect(deletePresetBtn, &QPushButton::clicked, this, &VoicePage::deletePreset);
    connect(savePresetBtn, &QPushButton::clicked, this, &VoicePage::saveCurrentPreset);
}


// **************** 训练向导弹窗逻辑 (独立 QDialog) ****************
void VoicePage::openTrainingConfigDialog() {
    QDialog dialog(this);

    dialog.setWindowTitle("🚀 训练向导");
    dialog.resize(580, 580);
    //弹窗样式表
    dialog.setStyleSheet(R"(
        QDialog { background-color: #f3f5f8; font-family: "Segoe UI", "Microsoft YaHei"; font-size: 13px; }
        QGroupBox {
            background-color: white;
            font-weight: bold;
            border: 1px solid #e0e0e0;
            border-radius: 8px;
            margin-top: 20px;     /* 为标题留出外部高度，防止被上方部件切断 */
            padding-top: 5px;
            padding-bottom: 5px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 15px;
            padding: 0 5px;       /* 左右覆盖一点边框 */
            color: #0078d4;
        }
        QLineEdit, QSpinBox { border: 1px solid #dcdfe6; border-radius: 4px; padding: 4px 8px; }
        QPushButton { background-color: white; border: 1px solid #dcdfe6; border-radius: 4px; padding: 6px 12px; }
        QPushButton:hover { background-color: #f5f7fa; color: #0078d4; border-color: #c0c4cc; }
        QTabWidget::pane { border: 1px solid #ddd; background: white; }
    )");

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(20, 10, 20, 20);
    mainLayout->setSpacing(15);

    QSettings settings("Yachi", "PersistentData");
    QString lastPyPath = settings.value("Voice_Train_PyPath", "").toString();
    QString lastDataset = settings.value("Voice_Train_Dataset", "").toString();

    // --- 第一步：环境配置 ---
    QGroupBox *step1Group = new QGroupBox("1️⃣ 第一步：配置 Python 运行环境");
    QVBoxLayout *s1Layout = new QVBoxLayout(step1Group);
    s1Layout->setContentsMargins(15, 15, 15, 15);

    QHBoxLayout *pyEnvLayout = new QHBoxLayout();
    QLineEdit *pyPathEdit = new QLineEdit();
    pyPathEdit->setText(lastPyPath);
    pyPathEdit->setPlaceholderText("例如: D:/GPT-SoVITS/runtime/python.exe");
    pyPathEdit->setMinimumHeight(32);

    QPushButton *btnAutoDetect = new QPushButton("🔍 自动检测");
    btnAutoDetect->setStyleSheet("background-color: #f0f8ff; color: #0277bd; font-weight: bold; border: 1px solid #81d4fa;");
    QPushButton *btnPyPath = new QPushButton("手动浏览");

    pyEnvLayout->addWidget(pyPathEdit, 1);
    pyEnvLayout->addWidget(btnAutoDetect);
    pyEnvLayout->addWidget(btnPyPath);
    s1Layout->addLayout(pyEnvLayout);
    mainLayout->addWidget(step1Group);

    // --- 第二步：准备数据集 ---
    QGroupBox *step2Group = new QGroupBox("2️⃣ 第二步：准备声音数据集 (切片与打标)");
    QVBoxLayout *s2Layout = new QVBoxLayout(step2Group);
    s2Layout->setContentsMargins(15, 15, 15, 15);

    QTabWidget *datasetTabs = new QTabWidget();
    datasetTabs->setStyleSheet("QTabBar::tab { padding: 6px 12px; }");

    //选项A: 我没有数据集（自动化流水线）
    QWidget *tabAutoMake = new QWidget();
    QVBoxLayout *autoMakeLayout = new QVBoxLayout(tabAutoMake);

    QLabel *makeTip = new QLabel("<span style='color:#666; font-size:12px;'>导入任意长音频/视频，自动执行 <b>UVR5(去伴奏) -> Slicer(切片) -> ASR(生成标注)</b></span>");
    makeTip->setWordWrap(true);
    autoMakeLayout->addWidget(makeTip);

    QHBoxLayout *rawLayout = new QHBoxLayout();
    QLineEdit *rawAudioPathEdit = new QLineEdit();
    rawAudioPathEdit->setPlaceholderText("选择包含人声的原始素材 (.wav / .mp4...)");
    rawAudioPathEdit->setMinimumHeight(32);
    QPushButton *btnRawAudio = new QPushButton("浏览素材");
    rawLayout->addWidget(rawAudioPathEdit, 1);
    rawLayout->addWidget(btnRawAudio);
    autoMakeLayout->addLayout(rawLayout);

    QHBoxLayout *langLayout = new QHBoxLayout();
    QComboBox *mediaLangCombo = new QComboBox();
    mediaLangCombo->addItems({"中文", "日本語", "English", "한국어"});
    mediaLangCombo->setCurrentText(settings.value("Voice_Train_MediaLang", "中文").toString());
    langLayout->addWidget(new QLabel("素材语种:"));
    langLayout->addWidget(mediaLangCombo, 1);
    autoMakeLayout->insertLayout(2,langLayout);

    QPushButton *btnProcessData = new QPushButton("✂️ 开始处理");
    btnProcessData->setStyleSheet("background-color: #e8f5e9; color: #2e7d32; font-weight: bold; border: 1px solid #a5d6a7; padding: 6px;");
    autoMakeLayout->addWidget(btnProcessData);
    autoMakeLayout->addStretch();
    datasetTabs->addTab(tabAutoMake, "✨ 我没有数据集");

    //选项B: 我有数据集（导入已有数据集）
    QWidget *tabExist = new QWidget();
    QVBoxLayout *existLayout = new QVBoxLayout(tabExist);

    QLabel *existTip = new QLabel("<span style='color:#666; font-size:12px;'>如果你已经使用其他工具处理好了纯净切片和 list.txt，直接在此指定输出文件夹即可。</span>");
    existTip->setWordWrap(true);
    existLayout->addWidget(existTip);

    QHBoxLayout *existDirLayout = new QHBoxLayout();
    QLineEdit *datasetPathEdit = new QLineEdit();
    datasetPathEdit->setText(lastDataset);
    datasetPathEdit->setPlaceholderText("指定或自动生成的标准数据集文件夹");
    datasetPathEdit->setMinimumHeight(32);
    QPushButton *btnDataset = new QPushButton("浏览文件夹");
    existDirLayout->addWidget(datasetPathEdit, 1);
    existDirLayout->addWidget(btnDataset);
    existLayout->addLayout(existDirLayout);
    existLayout->addStretch();
    datasetTabs->addTab(tabExist, "📁 我有数据集");

    s2Layout->addWidget(datasetTabs);
    mainLayout->addWidget(step2Group);
    // --- 声音数据集流水线 配置选项 ---
    QGroupBox *asrGroup = new QGroupBox("模型训练可配置项");
    QVBoxLayout *asrLayout = new QVBoxLayout(asrGroup);
    asrLayout->setContentsMargins(15, 15, 15, 15);
    asrLayout->setSpacing(10); //增加部件间的间距

    QLabel *paramRowTip = new QLabel(
        "<span style='color:#666; font-size:12px;'>"
        "💡模型规模越大文本识别越好，下载时间会长一些，后面可以手动修改识别有误的地方。"
        "</span>"
        );
    paramRowTip->setWordWrap(true);
    asrLayout->addWidget(paramRowTip);

    QHBoxLayout *paramRow = new QHBoxLayout();
    QComboBox *modelSizeCombo = new QComboBox();
    modelSizeCombo->addItems({"base", "small", "medium", "large-v3"});
    modelSizeCombo->setCurrentText(settings.value("Voice_Train_ModelSize", "small").toString());

    QCheckBox *cudaCheck = new QCheckBox("使用 CUDA 加速");
    cudaCheck->setChecked(settings.value("Voice_Train_UseCuda", false).toBool());

    paramRow->addWidget(new QLabel("ASR 模型:"));
    paramRow->addWidget(modelSizeCombo, 1);
    paramRow->addWidget(cudaCheck);
    asrLayout->addLayout(paramRow);

    QLabel *promptTip = new QLabel(
        "<span style='color:#666; font-size:12px;'>"
        "💡写下面这个能提高识别准确率，可以不写，写可以写涉及的角色名、特定名词、或语气词等。"
        "</span>"
        );
    promptTip->setWordWrap(true);
    asrLayout->addWidget(promptTip);

    QHBoxLayout *promptLayout = new QHBoxLayout();
    QLineEdit *initialPromptEdit = new QLineEdit();
    initialPromptEdit->setPlaceholderText("建议在20~100字间！例如写一段：“角色名是月見（ルナミ）ヤチヨ。”");
    initialPromptEdit->setMinimumHeight(32);

    promptLayout->addWidget(new QLabel("ASR 引导文本:"));
    promptLayout->addWidget(initialPromptEdit, 1);
    asrLayout->addLayout(promptLayout);

    QPushButton *btnConfirmConfig = new QPushButton("✅ 确认设置");
    btnConfirmConfig->setMinimumHeight(32);
    btnConfirmConfig->setStyleSheet("background-color: #f0f7ff; color: #005a9e; font-weight: bold; border: 1px solid #0078d4;");
    asrLayout->addWidget(btnConfirmConfig);

    mainLayout->addWidget(asrGroup);

    // --- 第三步：训练参数 ---
    QGroupBox *step3Group = new QGroupBox("3️⃣ 第三步：训练模型");
    QHBoxLayout *s3Layout = new QHBoxLayout(step3Group);
    s3Layout->setContentsMargins(15, 15, 15, 15);

    QLabel *modelTip = new QLabel("<span style='color:#666; font-size:12px;'>可以使用默认参数。</span>");
    s3Layout->addWidget(modelTip);

    QSpinBox *epochSpin = new QSpinBox();
    epochSpin->setRange(1, 1000);
    epochSpin->setValue(settings.value("Voice_Train_Epochs", 15).toInt());
    epochSpin->setPrefix("轮数(Epochs): ");
    epochSpin->setMinimumHeight(32);

    QSpinBox *batchSpin = new QSpinBox();
    batchSpin->setRange(1, 64);
    batchSpin->setValue(settings.value("Voice_Train_Batch", 4).toInt());
    batchSpin->setPrefix("批大小(Batch): ");
    batchSpin->setMinimumHeight(32);

    s3Layout->addWidget(epochSpin, 1);
    s3Layout->addWidget(batchSpin, 1);
    mainLayout->addWidget(step3Group);

    mainLayout->addStretch();

    // --- 底部操作栏 ---
    QHBoxLayout *actionLayout = new QHBoxLayout();
    QPushButton *cancelBtn = new QPushButton("取消");
    cancelBtn->setMinimumHeight(36);
    QPushButton *startBtn = new QPushButton("🔥 确认配置并启动流程");
    startBtn->setMinimumHeight(36);
    startBtn->setStyleSheet("background-color: #ff9800; color: white; font-weight: bold; border: none; border-radius: 4px; padding: 0 20px;");

    actionLayout->addStretch();
    actionLayout->addWidget(cancelBtn);
    actionLayout->addWidget(startBtn);
    mainLayout->addLayout(actionLayout);

    // ==========================================
    // 弹窗内部交互逻辑
    // ==========================================
    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    connect(btnPyPath, &QPushButton::clicked, [&]() {
        QString path = QFileDialog::getOpenFileName(&dialog, "选择 Python 解释器", "", "Python (python.exe);;All Files (*)");
        if (!path.isEmpty()) pyPathEdit->setText(path);
    });
    connect(btnRawAudio, &QPushButton::clicked, [&]() {
        QString path = QFileDialog::getOpenFileName(&dialog, "选择原始素材", "", "Media Files (*.wav *.mp3 *.flac *.mp4 *.mkv)");
        if (!path.isEmpty()) rawAudioPathEdit->setText(path);
    });
    connect(btnDataset, &QPushButton::clicked, [&]() {
        QString dir = QFileDialog::getExistingDirectory(&dialog, "选择数据集文件夹");
        if (!dir.isEmpty()) datasetPathEdit->setText(dir);
    });

    //自动检测 Python
    connect(btnAutoDetect, &QPushButton::clicked, [&]() {
        QString appPath = QCoreApplication::applicationDirPath();
        QStringList possiblePaths = {
            appPath + "/runtime/python.exe",
            appPath + "/env/python.exe",
            appPath + "/../runtime/python.exe",
            QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/miniconda3/envs/gptsovits/python.exe"
        };
        bool found = false;
        for (const QString &path : possiblePaths) {
            if (QFile::exists(path)) {
                pyPathEdit->setText(path);
                QMessageBox::information(&dialog, "检测成功", "🎉 已自动发现自带的 Python 环境！\n路径：" + path);
                found = true;
                break;
            }
        }
        if (!found) {
            //如果没找到，询问用户是否自动安装
            auto result = QMessageBox::question(&dialog, "未检测到环境",
                                                "未在默认路径找到 Python 环境。\n是否需要为您自动下载并安装？ (约 238MB)？",
                                                QMessageBox::Yes | QMessageBox::No);

            if (result == QMessageBox::Yes) {
                startAutomaticPythonInstall(pyPathEdit);
            }
        }
    });

    //模型训练可配置项 确认按钮
    connect(btnConfirmConfig, &QPushButton::clicked, [&]() {
        settings.setValue("Voice_Train_ModelSize", modelSizeCombo->currentText());
        settings.setValue("Voice_Train_UseCuda", cudaCheck->isChecked());
        settings.setValue("Voice_Train_ASRPrompt", initialPromptEdit->text());
        settings.setValue("Voice_Train_UVR5", true); //强制设为 true，默认必走 UVR5

        QMessageBox::information(&dialog, "配置保存", "已保存！");
    });

    //配置自动化数据处理管道
    connect(btnProcessData, &QPushButton::clicked, [&]() {
        if (pyPathEdit->text().isEmpty() || rawAudioPathEdit->text().isEmpty()) {
            QMessageBox::warning(&dialog, "配置不全", "请确保已配置 Python 环境并选择了原始素材文件！");
            return;
        }

        // 1.自动推导输出目录
        QFileInfo fi(rawAudioPathEdit->text());
        QString outDir = fi.absolutePath() + "/" + fi.baseName() + "_dataset";
        datasetPathEdit->setText(outDir);

        // 2.将当前关键参数立即写入配置，确保主流程能读取到最新值
        settings.setValue("Voice_Train_MediaLang", mediaLangCombo->currentText());
        settings.setValue("Voice_Train_ModelSize", modelSizeCombo->currentText());
        settings.setValue("Voice_Train_UseCuda", cudaCheck->isChecked());
        settings.setValue("Voice_Train_UVR5", true);
        settings.setValue("Voice_Train_ASRPrompt", initialPromptEdit->text());
        settings.setValue("Voice_Train_NeedPreprocess", true); //标记需要执行流水线

        // 3.直接以Accepted状态关闭弹窗
        dialog.accept();
    });

    //启动总体流程：仅保存参数并返回验证状态
    connect(startBtn, &QPushButton::clicked, [&]() {
        if (pyPathEdit->text().isEmpty() || datasetPathEdit->text().isEmpty()) {
            QMessageBox::warning(&dialog, "参数缺失", "请先完善 Python 路径和最终的数据集路径！");
            return;
        }
        settings.setValue("Voice_Train_PyPath", pyPathEdit->text());
        settings.setValue("Voice_Train_Dataset", datasetPathEdit->text());
        settings.setValue("Voice_Train_Epochs", epochSpin->value());
        settings.setValue("Voice_Train_Batch", batchSpin->value());

        dialog.accept();  //关闭窗口返回主界面，执行权在外部流程
    });

    // --- 返回主界面渲染日志、与执行流水线 ---
    if (dialog.exec() == QDialog::Accepted) {
        trainLogConsole->clear();

        // 1. 从 QSettings 提取用户在弹窗中确定的最新参数
        QString pyPath = settings.value("Voice_Train_PyPath").toString();
        QString datasetPath = settings.value("Voice_Train_Dataset").toString();
        int epochs = settings.value("Voice_Train_Epochs", 15).toInt();
        int batch = settings.value("Voice_Train_Batch", 4).toInt();
        bool needPre = settings.value("Voice_Train_NeedPreprocess", false).toBool();

        // 2. 启动自动化依赖检查（异步流程）
        // 注意：这里需要捕获所有后续逻辑需要的变量
        ensureDependenciesInstalled(pyPath, [this, pyPath, datasetPath, epochs, batch, needPre](bool success) {
            if (!success) {
                trainLogConsole->append(">> ❌ [中止] 环境依赖不足，无法启动后续流程。");
                return;
            }

            // 3. 安全机制：强行终结可能正在运行的旧进程，防止资源占用
            if (m_trainingProcess && m_trainingProcess->state() != QProcess::NotRunning) {
                trainLogConsole->append(">> [系统] 检测到旧任务，正在强制结束进程...");
                m_trainingProcess->kill();
                m_trainingProcess->waitForFinished(1000);
            }

            // 4. 根据标志位决定进入哪个流水线阶段[cite: 5]
            if (needPre) {
                // 阶段 1：执行数据集准备（UVR5 -> Slicer -> ASR）[cite: 5, 6]
                QSettings settings("Yachi", "PersistentData");
                settings.setValue("Voice_Train_NeedPreprocess", false); // 消费标志位[cite: 5]

                QString rawFile = settings.value("Voice_Train_RawAudio").toString();

                trainLogConsole->append(QString(">> [运行环境] Python解析器: %1").arg(pyPath));
                trainLogConsole->append(QString(">> [任务调度] 正在拉起自动化处理管道..."));
                trainLogConsole->append(QString(">> [输入文件]: %1").arg(rawFile));
                trainLogConsole->append(QString(">> [输出目录]: %1").arg(datasetPath));

                runDatasetPipeline(pyPath, rawFile, datasetPath);
            }
            else {
                // 阶段 2：直接开始模型训练[cite: 5, 6]
                trainLogConsole->append(QString(">> [任务调度] 侦测到已有数据集，直接拉起训练流水线..."));
                trainLogConsole->append(QString(">> [训练参数] Epochs: %1 | Batch: %2").arg(epochs).arg(batch));

                startModelTraining(pyPath, datasetPath, epochs, batch);
            }
        });
    }
}
// ********************************

// **************** 模型训练弹窗 ****************
///
/// \brief VoicePage::startAutomaticPythonInstall
/// \brief 自动下载并安装Python环境的接口
/// \param pyPathEdit
///
void VoicePage::startAutomaticPythonInstall(QLineEdit *pyPathEdit) {
    QString runtimeDir = QCoreApplication::applicationDirPath() + "/runtime";
    QDir().mkpath(runtimeDir);

    trainLogConsole->append(">> [系统] 准备从 GitHub 获取全量环境包...");

    //GitHub Release下载直链
    QUrl url("https://github.com/maomaonwn/Yachi/releases/download/Library/yachi-tts-runtime.zip");
    QNetworkRequest request(url);
    QNetworkReply *reply = networkManager->get(request);

    progressBar->setVisible(true);
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 recv, qint64 total) {
        if (total > 0) progressBar->setValue(static_cast<int>(recv * 100 / total));
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, runtimeDir, pyPathEdit]() {
        progressBar->setVisible(false);
        if (reply->error() == QNetworkReply::NoError) {
            QString zipPath = runtimeDir + "/yachi_runtime.zip";
            QFile file(zipPath);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(reply->readAll());
                file.close();

                trainLogConsole->append(">> [解压] 下载完成，正在部署全量环境...");

                QProcess *unzipProc = new QProcess(this);
                unzipProc->setWorkingDirectory(runtimeDir);

                // 【修复点】：在此处的捕获列表中添加了 unzipProc
                connect(unzipProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                        this, [this, runtimeDir, pyPathEdit, zipPath, unzipProc](int exitCode){
                            if (exitCode == 0) {
                                QString finalExe = runtimeDir + "/python.exe";
                                pyPathEdit->setText(finalExe);
                                trainLogConsole->append(">> [成功] 环境部署完毕！");
                                QFile::remove(zipPath);
                            } else {
                                trainLogConsole->append(">> [错误] 解压失败，请检查系统 tar 命令。");
                            }
                            unzipProc->deleteLater();
                        });

                unzipProc->start("tar", {"-xf", "yachi_runtime.zip"});
            }
        } else {
            trainLogConsole->append(">> [错误] 网络请求失败：" + reply->errorString());
        }
        reply->deleteLater();
    });
}

///
/// \brief VoicePage::ensureDependenciesInstalled
/// \brief 检查并安装依赖
/// \param pyPath
/// \param callback
///
void VoicePage::ensureDependenciesInstalled(const QString &pyPath, std::function<void(bool)> callback) {
    //检查下核心库是否存在
    //如果存在 pydub 文件夹，则直接跳过冗长的 pip install
    QString sitePackages = QFileInfo(pyPath).absolutePath() + "/Lib/site-packages/pydub";

    if (QDir(sitePackages).exists()) {
        trainLogConsole->append(">> [检查] 核心依赖库已就绪，跳过安装。");
        callback(true);
        return;
    }

    //否则，执行一次静默修复
    trainLogConsole->append(">> [环境] 正在检查缺失依赖...");
    QProcess *pipProc = new QProcess(this);
    pipProc->start(pyPath, {"-m", "pip", "install", "pydub", "librosa", "numpy", "-i", "https://pypi.tuna.tsinghua.edu.cn/simple"});

    connect(pipProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [=](int exitCode){
        callback(true); //无论结果如何尝试继续，因为py脚本内也有自愈逻辑
        pipProc->deleteLater();
    });
}

void VoicePage::runDatasetPipeline(const QString &pyPath, const QString &rawAudio, const QString &outDir) {
    QString scriptPath = QCoreApplication::applicationDirPath() + "/tools/make_dataset.py";

    // 1.清理旧进程
    if (m_trainingProcess) {
        m_trainingProcess->kill();
        m_trainingProcess->deleteLater();
    }
    m_trainingProcess = new QProcess(this);
    m_trainingProcess->setProcessChannelMode(QProcess::MergedChannels);

    // 2.从注册表读取最新的相关配置
    QSettings settings("Yachi", "PersistentData");
    QString modelSize = settings.value("Voice_Train_ModelSize", "medium").toString(); //默认为 small
    bool useCuda = settings.value("Voice_Train_UseCuda", false).toBool();
    bool useUvr5 = settings.value("Voice_Train_UVR5", true).toBool(); //默认开启 UVR5
    QString mediaLang = settings.value("Voice_Train_MediaLang", "中文").toString();

    // 3.组装命令行参数（传递给make_dataset.py使用）
    QStringList args;
    args << scriptPath
         << "--input" << rawAudio
         << "--output" << outDir
         << "--model_size" << modelSize
         << "--language" << mediaLang;

    if (useUvr5) args << "--uvr5";
    if (useCuda) args << "--use_cuda";

    // 4.绑定输出信号
    connect(m_trainingProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        trainLogConsole->append(QString::fromUtf8(m_trainingProcess->readAllStandardOutput()).trimmed());
    });

    // 5.绑定结束信号，用于弹出网页编辑器
    connect(m_trainingProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, outDir](int exitCode) {
                if (exitCode == 0) {
                    trainLogConsole->append("\n>> [完成] 数据集准备完毕！正在为您打开标注编辑器...");

                    //自动拉起浏览器进行人工核对
                    QString listPath = outDir + "/list.txt";
                    openExternalLabelEditor(listPath);

                    trainLogConsole->append(">> 💡 提示：确认 list.txt 后，即可开启下一步训练。");
                } else {
                    trainLogConsole->append("\n>> ❌ [失败] 处理进程非正常退出，请检查上方 Python 报错信息。");
                }

                //任务结束，清理句柄
                m_trainingProcess->deleteLater();
                m_trainingProcess = nullptr;
            });
    // 6.启动进程
    trainLogConsole->append(">> [启动] 正在以参数: " + args.join(" ") + " 启动 Python 脚本...");
    m_trainingProcess->start(pyPath, args);
}

void VoicePage::startModelTraining(const QString &pyPath, const QString &datasetPath, int epochs, int batch) {
    //根据你实际的底层项目结构(GPT-SoVITS等)，这里指向实际的训练脚本
    QString scriptPath = QCoreApplication::applicationDirPath() + "/tools/train_model.py";

    m_trainingProcess = new QProcess(this);
    m_trainingProcess->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_trainingProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        trainLogConsole->append(QString::fromLocal8Bit(m_trainingProcess->readAllStandardOutput()).trimmed());
        QTextCursor cursor = trainLogConsole->textCursor();
        cursor.movePosition(QTextCursor::End);
        trainLogConsole->setTextCursor(cursor);
    });

    connect(m_trainingProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode) {
                if (exitCode == 0) {
                    trainLogConsole->append("\n>> 🎉 [成功] 模型训练流水线圆满完成！");
                    trainLogConsole->append(">> 请切换至“模型导入”页签，选择刚生成的 .ckpt (GPT) 和 .pth (SoVITS) 文件进行效果测试。");
                } else {
                    trainLogConsole->append("\n>> ❌ [错误] 训练进程非正常退出，请检查环境配置或上方日志报错。");
                }
                m_trainingProcess->deleteLater();
                m_trainingProcess = nullptr;
            });

    //组装参数队列
    QStringList args;
    args << scriptPath
         << "--dataset" << datasetPath
         << "--epochs" << QString::number(epochs)
         << "--batch_size" << QString::number(batch);

    //发射火箭
    m_trainingProcess->start(pyPath, args);
}

void VoicePage::openExternalLabelEditor(const QString &listPath) {
    QFile file(listPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        trainLogConsole->append(">> [错误] 无法读取标注文件: " + listPath);
        return;
    }

    // 1.生成HTML
    QString htmlContent = R"raw(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Yachi 标注编辑器</title>
    <style>
        body { font-family: 'Segoe UI', sans-serif; background: #f5f7fa; padding: 20px; }
        .container { max-width: 1000px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h2 { color: #0078d4; }
        table { width: 100%; border-collapse: collapse; margin-top: 20px; }
        th, td { border: 1px solid #eee; padding: 12px; text-align: left; }
        th { background: #f8f9fa; }
        input { width: 95%; padding: 8px; border: 1px solid #ddd; border-radius: 4px; font-size: 14px; }
        .save-area { position: sticky; top: 0; background: white; padding: 15px 0; border-bottom: 2px solid #0078d4; margin-bottom: 10px; z-index: 100; }
        button { background: #28a745; color: white; border: none; padding: 10px 20px; border-radius: 4px; cursor: pointer; font-weight: bold; }
        button:hover { background: #218838; }
    </style>
</head>
<body>
    <div class="container">
        <div class="save-area">
            <h2>📝 标注内容核对</h2>
            <p>如有修改，修改完成后，点击按钮复制结果并手动粘贴回 list.txt 文件。</p>
            <button onclick="copyToClipboard()">复制修改后的全部内容</button>
        </div>
        <table>
            <tr><th>音频预览</th><th>转写文本 (可修改)</th></tr>
)raw";

    // 2.遍历读取标注文件内容并构建表格行
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        QStringList parts = line.split("|"); // GPT-SoVITS 格式: 路径|角色|语种|内容
        if (parts.size() >= 4) {
            QString audioPath = parts[0];
            QString text = parts[3];

            //将路径转为浏览器可识别的 file 协议路径
            QString webAudioPath = "file:///" + audioPath.replace("\\", "/");
            //前三部分（路径|角色|语种|）作为前缀保留
            QString prefix = parts.mid(0, 3).join("|") + "|";

            htmlContent += QString(
                               "<tr>"
                               "  <td><audio controls src=\"%1\" style=\"height:30px; width:220px;\"></audio></td>"
                               "  <td><input type=\"text\" class=\"label-input\" data-prefix=\"%2\" value=\"%3\"></td>"
                               "</tr>"
                               ).arg(webAudioPath).arg(prefix).arg(text.replace("\"", "&quot;")); // 转义文本中的双引号
        }
    }

    // 3.注入JS逻辑并闭合标签
    htmlContent += R"raw(
        </table>
    </div>
    <script>
        function copyToClipboard() {
            const inputs = document.querySelectorAll('.label-input');
            let output = [];
            inputs.forEach(input => {
                output.push(input.getAttribute('data-prefix') + input.value);
            });
            const text = output.join('\n');

            // 使用标准的 Clipboard API 复制[cite: 26]
            navigator.clipboard.writeText(text).then(() => {
                alert('内容已成功复制到剪贴板！\n请打开 list.txt，全选并粘贴覆盖即可。');
            }).catch(err => {
                console.error('无法复制: ', err);
                alert('复制失败，请查看控制台错误。');
            });
        }
    </script>
</body>
</html>
)raw";

    // 4.保存为临时文件并调用默认浏览器
    QString tempPath = QDir::tempPath() + "/yachi_label_editor.html";
    QFile outFile(tempPath);
    if (outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&outFile);
        out << htmlContent;
        outFile.close();

        //使用QDesktopServices打开
        QDesktopServices::openUrl(QUrl::fromLocalFile(tempPath));
        trainLogConsole->append(">> [系统] 已在浏览器中打开标注编辑器。路径: " + tempPath);
    }
}

// ********************************

// **************** 预设管理逻辑 ****************
void VoicePage::refreshPresetList() {
    QSettings settings("Yachi", "PersistentData");
    QString current = presetCombo->currentText();
    settings.beginGroup("Voice_ParamPresets");
    QStringList keys = settings.childGroups();
    settings.endGroup();

    if (keys.isEmpty()) {
        QString defaultName = "默认参数";
        settings.beginGroup("Voice_ParamPresets/" + defaultName);
        settings.setValue("speed", 1.0);
        settings.setValue("pitch", 0.0);
        settings.setValue("interval", 0.5);
        settings.setValue("temp", 0.7);
        settings.setValue("topP", 0.8);
        settings.setValue("topK", 50);
        settings.endGroup();
        keys.append(defaultName);
    }

    presetCombo->blockSignals(true);
    presetCombo->clear();
    presetCombo->addItems(keys);
    if (keys.contains(current)) presetCombo->setCurrentText(current);
    else if (!keys.isEmpty()) presetCombo->setCurrentIndex(0);
    presetCombo->blockSignals(false);

    loadSelectedPreset();
}

void VoicePage::loadSelectedPreset() {
    QString name = presetCombo->currentText();
    if (name.isEmpty()) return;

    QSettings settings("Yachi", "PersistentData");
    settings.beginGroup("Voice_ParamPresets/" + name);
    speedSpin->setValue(settings.value("speed", 1.0).toDouble());
    pitchSpin->setValue(settings.value("pitch", 0.0).toDouble());
    intervalSpin->setValue(settings.value("interval", 0.5).toDouble());
    tempSpin->setValue(settings.value("temp", 0.7).toDouble());
    topPSpin->setValue(settings.value("topP", 0.8).toDouble());
    topKSpin->setValue(settings.value("topK", 50).toInt());
    settings.endGroup();
}

void VoicePage::addNewPreset() {
    QSettings settings("Yachi", "PersistentData");
    settings.beginGroup("Voice_ParamPresets");
    QStringList keys = settings.childGroups();
    settings.endGroup();

    int count = 1;
    QString newName;
    while(true) {
        newName = QString("自定义预设 %1").arg(count++);
        if (!keys.contains(newName)) break;
    }

    settings.beginGroup("Voice_ParamPresets/" + newName);
    settings.setValue("speed", speedSpin->value());
    settings.setValue("pitch", pitchSpin->value());
    settings.setValue("interval", intervalSpin->value());
    settings.setValue("temp", tempSpin->value());
    settings.setValue("topP", topPSpin->value());
    settings.setValue("topK", topKSpin->value());
    settings.endGroup();

    refreshPresetList();
    presetCombo->setCurrentText(newName);
}

void VoicePage::renamePreset() {
    QString oldName = presetCombo->currentText();
    if (oldName.isEmpty()) return;
    bool ok;
    QString newName = QInputDialog::getText(this, "重命名", "输入新预设名称:", QLineEdit::Normal, oldName, &ok);
    if (ok && !newName.isEmpty()) {
        QSettings settings("Yachi", "PersistentData");

        settings.beginGroup("Voice_ParamPresets/" + oldName);
        double speed = settings.value("speed").toDouble();
        double pitch = settings.value("pitch").toDouble();
        double interval = settings.value("interval").toDouble();
        double temp = settings.value("temp").toDouble();
        double topP = settings.value("topP").toDouble();
        int topK = settings.value("topK").toInt();
        settings.endGroup();

        settings.remove("Voice_ParamPresets/" + oldName);

        settings.beginGroup("Voice_ParamPresets/" + newName);
        settings.setValue("speed", speed);
        settings.setValue("pitch", pitch);
        settings.setValue("interval", interval);
        settings.setValue("temp", temp);
        settings.setValue("topP", topP);
        settings.setValue("topK", topK);
        settings.endGroup();

        refreshPresetList();
        presetCombo->setCurrentText(newName);
    }
}

void VoicePage::deletePreset() {
    QString name = presetCombo->currentText();
    if (name.isEmpty()) return;
    if (QMessageBox::question(this, "删除", "确认删除该参数预设？") == QMessageBox::Yes) {
        QSettings settings("Yachi", "PersistentData");
        settings.remove("Voice_ParamPresets/" + name);
        refreshPresetList();
    }
}

void VoicePage::saveCurrentPreset() {
    QString name = presetCombo->currentText();
    if (name.isEmpty()) return;
    QSettings settings("Yachi", "PersistentData");
    settings.beginGroup("Voice_ParamPresets/" + name);
    settings.setValue("speed", speedSpin->value());
    settings.setValue("pitch", pitchSpin->value());
    settings.setValue("interval", intervalSpin->value());
    settings.setValue("temp", tempSpin->value());
    settings.setValue("topP", topPSpin->value());
    settings.setValue("topK", topKSpin->value());
    settings.endGroup();

    QMessageBox::information(this, "成功", "参数预设已更新保存。");
}
// ********************************

// **************** 辅助函数与底层交互 ****************
QHBoxLayout* VoicePage::createSliderWithSpinBox(const QString &label, double min, double max, double step, double defaultVal, QSlider*& outSlider, QDoubleSpinBox*& outSpin, int decimals) {
    QHBoxLayout *layout = new QHBoxLayout();
    outSlider = new QSlider(Qt::Horizontal);
    outSpin = new QDoubleSpinBox();

    int scale = 100;
    outSlider->setRange(min * scale, max * scale);
    outSlider->setValue(defaultVal * scale);

    outSpin->setRange(min, max);
    outSpin->setSingleStep(step);
    outSpin->setValue(defaultVal);
    outSpin->setMinimumWidth(80);

    //保留多少位小数位数
    outSpin->setDecimals(decimals);

    //双向绑定信号
    connect(outSlider, &QSlider::valueChanged, [outSpin, scale](int val){ outSpin->setValue((double)val / scale); });
    connect(outSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [outSlider, scale](double val){ outSlider->setValue(val * scale); });

    layout->addWidget(new QLabel(label));
    layout->addWidget(outSlider, 1);
    layout->addWidget(outSpin);
    return layout;
}

QHBoxLayout* VoicePage::createIntSliderWithSpinBox(const QString &label, int min, int max, int defaultVal, QSlider*& outSlider, QSpinBox*& outSpin) {
    QHBoxLayout *layout = new QHBoxLayout();
    outSlider = new QSlider(Qt::Horizontal);
    outSlider->setRange(min, max);
    outSlider->setValue(defaultVal);

    outSpin = new QSpinBox();
    outSpin->setRange(min, max);
    outSpin->setValue(defaultVal);
    outSpin->setMinimumWidth(80);

    connect(outSlider, &QSlider::valueChanged, outSpin, &QSpinBox::setValue);
    connect(outSpin, QOverload<int>::of(&QSpinBox::valueChanged), outSlider, &QSlider::setValue);

    layout->addWidget(new QLabel(label));
    layout->addWidget(outSlider, 1);
    layout->addWidget(outSpin);
    return layout;
}

void VoicePage::initAudio() {
    mediaPlayer = new QMediaPlayer(this);
    audioOutput = new QAudioOutput(this);
    mediaPlayer->setAudioOutput(audioOutput);
    audioBuffer = new QBuffer(this);

    connect(mediaPlayer, &QMediaPlayer::positionChanged, this, &VoicePage::updatePlaybackProgress);
    connect(mediaPlayer, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state){
        playBtn->setText(state == QMediaPlayer::PlayingState ? "⏸ 暂停" : "▶ 播放");
    });
}

void VoicePage::onBrowseCachePath() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择音频缓存目录", cachePathEdit->text());
    if (!dir.isEmpty()) {
        cachePathEdit->setText(dir);
    }
}

void VoicePage::onGenerateClicked() {
    QString text = inputText->toPlainText().trimmed();
    if (text.isEmpty()) return;
    // --- 实例化配置对象 ---
    VoiceConfig config;
    config.text = text;
    config.lang = "zh";  //TODO: 语种后面弄下动态设置

    config.speed = speedSpin->value();
    config.temp = tempSpin->value();
    config.topP = topPSpin->value();
    config.topK = topKSpin->value();

    config.refAudioPath = refAudioPathEdit->text();
    config.promptText = refTextEdit->text();

    config.useCache = cacheGroup->isChecked();
    config.cacheDir = cachePathEdit->text();

    // --- 调用逻辑层 ---
    // 通过信号或直接调用，将任务下发给 m_voiceAgent
    m_voiceAgent->generate(config);

    // --- UI 反馈 ---
    generateBtn->setEnabled(false); // 防止重复点击造成网络拥堵
    progressBar->setVisible(true);
    progressBar->setRange(0, 0);    // 设置为动画忙碌状态 (Marquee 效果)[cite: 10]
}

void VoicePage::handleFinished() {
    progressBar->setVisible(false);
    generateBtn->setEnabled(true);

    if (m_audioData.isEmpty()) return;

    playBtn->setEnabled(true);
    exportBtn->setEnabled(true);

    if (audioBuffer->isOpen()) audioBuffer->close();
    audioBuffer->setData(m_audioData);
    audioBuffer->open(QIODevice::ReadOnly);
    mediaPlayer->setSourceDevice(audioBuffer);
}

void VoicePage::onPlayClicked() {
    if (mediaPlayer->playbackState() == QMediaPlayer::PlayingState) {
        mediaPlayer->pause();
    } else {
        mediaPlayer->play();
    }
}

void VoicePage::onExportClicked() {
    if (m_audioData.isEmpty()) return;

    QString format = exportFormatCombo->currentText();
    QString defaultFilter = QString("Audio Files (*%1)").arg(format);

    QString fileName = QFileDialog::getSaveFileName(this, "导出语音",
                                                    QStandardPaths::writableLocation(QStandardPaths::MusicLocation) + "/yachi_vocal" + format,
                                                    defaultFilter);

    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(m_audioData);
            file.close();
            QMessageBox::information(this, "成功", "语音文件已保存！");
        }
    }
}

void VoicePage::updatePlaybackProgress() {
    // 预留用于更新 UI 的进度条
}
// ********************************