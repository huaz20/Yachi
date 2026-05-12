#include "translationpage.h"
#include "urlstrategy.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QSettings>
#include <QInputDialog>
#include <QDateTime>
#include <QListWidget>
#include <QDialog>
#include <QCheckBox>
#include <QApplication>
#include <QTimer>
#include <QClipboard>
#include <QStackedWidget>

TranslationPage::TranslationPage(AgentCore *agent, QWidget *parent)
    : QWidget(parent), m_agent(agent)
{
    m_urlManager = new QNetworkAccessManager(this);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // --- 翻译预设管理UI (比例 1) ---
    QGroupBox *configGroup = new QGroupBox("翻译预设管理");
    QVBoxLayout *configLayout = new QVBoxLayout(configGroup);
    configLayout->setContentsMargins(10, 15, 10, 10);
    configLayout->setSpacing(8);

    QHBoxLayout *presetCtrlLayout = new QHBoxLayout();
    presetCombo = new QComboBox();
    presetCombo->setMinimumWidth(180);

    addPresetBtn = new QPushButton("+ 新建");
    renamePresetBtn = new QPushButton("重命名");
    deletePresetBtn = new QPushButton("删除");
    savePromptBtn = new QPushButton("保存当前内容");
    savePromptBtn->setStyleSheet("background-color: #e3f2fd; color: #0277bd; font-weight: bold; border: 1px solid #81d4fa;");

    presetCtrlLayout->addWidget(new QLabel("预设方案:"));
    presetCtrlLayout->addWidget(presetCombo, 1);
    presetCtrlLayout->addWidget(addPresetBtn);
    presetCtrlLayout->addWidget(renamePresetBtn);
    presetCtrlLayout->addWidget(deletePresetBtn);
    presetCtrlLayout->addSpacing(10);
    presetCtrlLayout->addWidget(savePromptBtn);
    configLayout->addLayout(presetCtrlLayout);

    promptEdit = new QPlainTextEdit();
    promptEdit->setPlaceholderText("在这里编辑提示词...");
    promptEdit->setMinimumHeight(100);
    promptEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    configLayout->addWidget(promptEdit);

    mainLayout->addWidget(configGroup, 1);
    // ------

    // --- 网页读取UI ---
    urlGroup = new QGroupBox("网页读取 (可选)");
    QVBoxLayout *urlLayout = new QVBoxLayout(urlGroup);

    QHBoxLayout *urlInputLayout = new QHBoxLayout();

    urlEdit = new QLineEdit();
    urlEdit = new QLineEdit();
    urlEdit->setPlaceholderText("请输入以 http:// 或 https:// 开头的网址...");
    fetchBtn = new QPushButton("抓取并导入");
    fetchBtn->setFixedWidth(100);
    fetchBtn->setStyleSheet("background-color: #4caf50; color: white; font-weight: bold; height: 28px;");
    filterStatusBtn = new QPushButton("✨智能过滤设置"
                                      ""
                                      ""
                                      ""
                                      ""
                                      ""
                                      ""
                                      "");
    filterStatusBtn->setCursor(Qt::PointingHandCursor);
    filterStatusBtn->setStyleSheet(R"(
        QPushButton {
                color: #2e7d32;
                background: transparent;
                border: 1px solid #a5d6a7;
                border-radius: 4px;
                padding: 2px 8px;
                font-size: 12px;
                font-weight: bold;
            }
            QPushButton:hover { background-color: #e8f5e9; }
        )");

    urlInputLayout->addWidget(urlEdit, 1);
    urlInputLayout->addWidget(fetchBtn);
    urlInputLayout->addWidget(filterStatusBtn);

    urlTipLabel = new QLabel("<font color='#888'>💡提示：部分动态渲染或反爬严格的网页（如推特/FB）可能无法读取。抓取由Jina Reader提供。</font>");
    urlTipLabel->setStyleSheet("font-size: 11px;");

    urlLayout->addLayout(urlInputLayout);
    urlLayout->addWidget(urlTipLabel);

    mainLayout->addWidget(urlGroup, 0);
    // ------

    // --- 中间工具栏 ---
    QHBoxLayout *toolBar = new QHBoxLayout();
    langCombo = new QComboBox();
    //选项
    langCombo->addItems({
        "简体中文",      // 中文
        "繁体中文",      // 下面按照Key A-Z的顺序排列
        "Afrikaans",    // 阿非利卡语
        "العربية",      // 阿拉伯语
        "বাংলা",         // 孟加拉语
        "Български",    // 保加利亚语
        "Català",       // 加泰罗尼亚语
        "Čeština",      // 捷克语
        "Dansk",        // 丹麦语
        "Deutsch",      // 德语
        "Ελληνικά",     // 希腊语
        "English",      // 英语
        "Español",      // 西班牙语
        "Eesti",        // 爱沙尼亚语
        "فارسی",        // 波斯语
        "Suomi",        // 芬兰语
        "Français",     // 法语
        "עברית",        // 希伯来语
        "हिन्दी",         // 印地语
        "Hrvatski",     // 克罗地亚语
        "Magyar",       // 匈牙利语
        "Indonesian",   // 印尼语
        "Italiano",     // 意大利语
        "日本語",        // 日语
        "Қазақша",      // 哈萨克语
        "한국어",        // 韩语
        "Lietuvių",     // 立陶宛语
        "Latviešu",     // 拉脱维亚语
        "Malay",        // 马来语
        "Nederlands",   // 荷兰语
        "Norsk",        // 挪威语
        "Polski",       // 波兰语
        "Português",    // 葡萄牙语
        "Română",       // 罗马尼亚语
        "Русский",      // 俄语
        "Srpski",       // 塞尔维亚语
        "Slovenčina",   // 斯洛伐克语
        "Slovenščina",  // 斯洛文尼亚语
        "Svenska",      // 瑞典语
        "ไทย",          // 泰语
        "Türkçe",       // 土耳其语
        "Українська",   // 乌克兰语
        "Tiếng Việt",   // 越南语
    });
    //固定大小
    langCombo->setFixedWidth(110);

    exportBtn = new QPushButton("导出当前结果 (.txt)");
    historyBtn = new QPushButton("历史记录");

    toolBar->addWidget(new QLabel("<b>目标语言:</b>"));
    toolBar->addWidget(langCombo);
    toolBar->addStretch();
    toolBar->addWidget(historyBtn);
    toolBar->addWidget(exportBtn);

    mainLayout->addLayout(toolBar, 0);
    // ------

    // --- 翻译文本区 (比例2) ---
    QHBoxLayout *textAreaLayout = new QHBoxLayout();
    sourceText = new QPlainTextEdit();
    sourceText->setPlaceholderText("请输入源文本...");

    targetText = new QPlainTextEdit();
    targetText->setReadOnly(true);
    targetText->setPlaceholderText("等待翻译结果...");
    targetText->setStyleSheet("background-color: #fcfcfc; border: 1px solid #eee;");

    translateBtn = new QPushButton("开始\n翻译");
    translateBtn->setFixedSize(75, 75);
    translateBtn->setObjectName("translateBtn");

    //中止翻译按钮
    stopBtn = new QPushButton("中止");
    stopBtn->setFixedSize(75, 75);
    stopBtn->setObjectName("stopBtn");
    stopBtn->setVisible(false); //初始状态隐藏，只有翻译时才显示
    stopBtn->setStyleSheet(R"(
    QPushButton#stopBtn {
        background-color: #d32f2f;
        color: white;
        border-radius: 37px;
        font-weight: bold;
        font-size: 14px;
        border: 2px solid #ffcdd2;
    }
    QPushButton#stopBtn:hover { background-color: #b71c1c; }
    QPushButton#stopBtn:pressed { background-color: #7f0000; }
)");

    btnStack = new QStackedWidget();
    btnStack->setFixedSize(85, 85); //给容器固定大小，防止布局跳动
    btnStack->addWidget(translateBtn); //Index 0：空闲状态
    btnStack->addWidget(stopBtn);      //Index 1：翻译状态
    btnStack->setCurrentIndex(0);
    QVBoxLayout *btnCtrlLayout = new QVBoxLayout();
    btnCtrlLayout->addWidget(btnStack);
    btnCtrlLayout->setAlignment(Qt::AlignCenter);

    textAreaLayout->addWidget(sourceText, 1);
    textAreaLayout->addLayout(btnCtrlLayout);
    textAreaLayout->addWidget(targetText, 1);

    mainLayout->addLayout(textAreaLayout, 2);

    // 样式美化与字体兼容
    this->setStyleSheet(R"(
        TranslationPage {
            background-color: white;
            font-family: "Segoe UI", "Microsoft YaHei", "Segoe UI Emoji", sans-serif;
        }
        QGroupBox { font-size: 13px; border: 1px solid #ddd; border-radius: 6px; margin-top: 10px; font-weight: bold; }
        QPushButton#translateBtn {
            background-color: #0078d4; color: white; border-radius: 37px; font-weight: bold; font-size: 14px;
        }
        QPushButton#translateBtn:hover { background-color: #005a9e; }
        QTextEdit { border: 1px solid #ccc; border-radius: 4px; padding: 5px; font-size: 14px; }
    )");
    // ------

    // --- 信号绑定 ---
    //网页读取
    connect(fetchBtn, &QPushButton::clicked, this, &TranslationPage::fetchUrlContent);
    connect(filterStatusBtn, &QPushButton::clicked, this, &TranslationPage::showFilterDetails);

    connect(addPresetBtn, &QPushButton::clicked, this, &TranslationPage::addNewPreset);
    connect(renamePresetBtn, &QPushButton::clicked, this, &TranslationPage::renamePreset);
    connect(deletePresetBtn, &QPushButton::clicked, this, &TranslationPage::deletePreset);
    connect(savePromptBtn, &QPushButton::clicked, this, &TranslationPage::saveCurrentPrompt);
        connect(presetCombo, &QComboBox::currentTextChanged, this, &TranslationPage::loadSelectedPrompt);

    connect(translateBtn, &QPushButton::clicked, this, &TranslationPage::doTranslate);
    connect(exportBtn, &QPushButton::clicked, this, &TranslationPage::exportToTxt);
    connect(historyBtn, &QPushButton::clicked, this, &TranslationPage::showHistory);
    connect(stopBtn, &QPushButton::clicked, this, &TranslationPage::abortTranslation);

    //流式输出的信号
    connect(m_agent, &AgentCore::partialResponseMsg, this, &TranslationPage::onPartialTranslationResult);
    connect(m_agent, &AgentCore::responseMsg, this, &TranslationPage::onChunkTranslationResult);
    connect(m_agent, &AgentCore::errorMsg, this, &TranslationPage::onTranslationError);
    // -------

    initTutorialPresets();
    refreshPresetList();
}

void TranslationPage::initTutorialPresets() {
    QSettings settings("Yachi", "PersistentData");
    if (!settings.contains("Trans_PromptPresets/预设1：基础翻译")) {
        settings.setValue("Trans_PromptPresets/预设1：基础翻译", "翻译用户提供的内容，不要有任何多余的解释。");
    }
}

void TranslationPage::saveToHistory(const QString &source, const QString &lang, const QString &result) {
    QSettings settings("Yachi", "PersistentData");
    int size = settings.beginReadArray("TranslationHistory");
    struct HistEntry { QString time, source, lang, result; };
    QList<HistEntry> list;
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        list.append({
            settings.value("time").toString(),
            settings.value("source").toString(),
            settings.value("lang").toString(),
            settings.value("result").toString()
        });
    }
    settings.endArray();

    list.prepend({QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"), source, lang, result});
    if(list.size() > 50) list.removeLast();

    settings.beginWriteArray("TranslationHistory");
    for (int i = 0; i < list.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("time", list[i].time);
        settings.setValue("source", list[i].source);
        settings.setValue("lang", list[i].lang);
        settings.setValue("result", list[i].result);
    }
    settings.endArray();
}

void TranslationPage::showHistory() {
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle("查询历史记录");
    dlg->resize(620, 480);
    QVBoxLayout *layout = new QVBoxLayout(dlg);

    QListWidget *listWidget = new QListWidget();
    QSettings settings("Yachi", "PersistentData");
    int size = settings.beginReadArray("TranslationHistory");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        QString srcPreview = settings.value("source").toString().left(25).replace("\n", " ");
        QString info = QString("[%1] 目标:%2 | %3...")
                           .arg(settings.value("time").toString(), settings.value("lang").toString(), srcPreview);
        listWidget->addItem(info);
    }
    settings.endArray();

    layout->addWidget(new QLabel("<b>双击条目查看详情：</b>"));
    layout->addWidget(listWidget);

    QHBoxLayout *btns = new QHBoxLayout();
    QPushButton *clearBtn = new QPushButton("清空历史记录");
    clearBtn->setStyleSheet("color: white; background-color: #d32f2f; border-radius: 4px; padding: 6px 12px; font-weight: bold;");
    QPushButton *closeBtn = new QPushButton("关闭");

    btns->addWidget(clearBtn);
    btns->addStretch();
    btns->addWidget(closeBtn);
    layout->addLayout(btns);

    connect(clearBtn, &QPushButton::clicked, this, [=](){
        if(QMessageBox::warning(dlg, "确认", "确定清空所有历史吗？", QMessageBox::Yes|QMessageBox::No) == QMessageBox::Yes) {
            QSettings s("Yachi", "PersistentData");
            s.remove("TranslationHistory");
            listWidget->clear();
        }
    });
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);

    // 详情弹窗逻辑
    connect(listWidget, &QListWidget::itemDoubleClicked, this, [=](QListWidgetItem *item){
        int idx = listWidget->row(item);
        QSettings s("Yachi", "PersistentData");
        s.beginReadArray("TranslationHistory");
        s.setArrayIndex(idx);
        QString timeStr = s.value("time").toString();
        QString langStr = s.value("lang").toString();
        QString srcStr = s.value("source").toString();
        QString resStr = s.value("result").toString();
        s.endArray();

        QDialog *det = new QDialog(dlg);
        det->setWindowTitle("翻译详情");
        det->resize(550, 600);
        QVBoxLayout *v = new QVBoxLayout(det);

        v->addWidget(new QLabel(QString("<b>记录时间:</b> %1  |  <b>语种:</b> %2").arg(timeStr, langStr)));

        v->addWidget(new QLabel("<b>原文内容:</b>"));
        QTextEdit *srcE = new QTextEdit();
        srcE->setPlainText(srcStr);
        srcE->setReadOnly(true);
        v->addWidget(srcE, 1);

        v->addWidget(new QLabel("<b>翻译结果:</b>"));
        QTextEdit *resE = new QTextEdit();
        resE->setPlainText(resStr);
        resE->setReadOnly(true);
        resE->setStyleSheet("background-color: #f8fbff;");
        v->addWidget(resE, 1);

        QHBoxLayout *detBtns = new QHBoxLayout();
        QPushButton *exportThisBtn = new QPushButton("导出此条结果 (.txt)");
        exportThisBtn->setStyleSheet("background-color: #4caf50; color: white; font-weight: bold; padding: 5px 15px;");

        QPushButton *okBtn = new QPushButton("确定");
        okBtn->setFixedWidth(80);

        detBtns->addWidget(exportThisBtn);
        detBtns->addStretch();
        detBtns->addWidget(okBtn);
        v->addLayout(detBtns);

        // 核心修复点：使用副本进行 replace 操作
        connect(exportThisBtn, &QPushButton::clicked, this, [=](){
            QString safeTime = timeStr; // 创建副本
            safeTime.replace(":", "-"); // 在副本上操作

            QString path = QFileDialog::getSaveFileName(det, "导出单条记录",
                                                        QString("翻译记录_%1").arg(safeTime),
                                                        "Text Files (*.txt)");
            if(!path.isEmpty()){
                QFile f(path);
                if(f.open(QIODevice::WriteOnly | QIODevice::Text)){
                    QTextStream out(&f);
                    out << "时间: " << timeStr << "\n";
                    out << "目标语言: " << langStr << "\n";
                    out << "--------------------------\n";
                    out << "【原文】\n" << srcStr << "\n\n";
                    out << "【结果】\n" << resStr << "\n";
                    f.close();
                    QMessageBox::information(det, "成功", "记录已成功导出。");
                }
            }
        });

        connect(okBtn, &QPushButton::clicked, det, &QDialog::accept);
        det->exec();
    });

    dlg->exec();
}

// **************** 翻译核心逻辑 ****************

///
/// \brief TranslationPage::doTranslate
/// \brief 启动翻译
///
void TranslationPage::doTranslate() {
    QString text = sourceText->toPlainText();
    if(text.isEmpty()) return;

    // 1.先分块
    //初始化状态
    m_accumulatedResult.clear();
    m_currentChunkResult.clear();
    //建议每块1500-2000字，留出足够的Token给AI输出
    m_chunkList = splitText(text, 2000);
    //记录待翻译块数
    m_totalChunks = m_chunkList.size();
    m_isProcessing = true;

    // 2.UI反馈
    translateBtn->setEnabled(false);
    btnStack->setCurrentIndex(1); //切换到“中止”按钮

    targetText->setPlainText(QString("翻译中（共 %1 块）...").arg(m_totalChunks));

    // 3.启动第一个块（递归按块翻译）
    processNextChunk();
}

///
/// \brief TranslationPage::processNextChunck
/// \brief 块内翻译逻辑
///
void TranslationPage::processNextChunk()
{
    //如果全部翻译完成
    if (m_chunkList.isEmpty()) {
        m_isProcessing = false;
        translateBtn->setEnabled(true);
        btnStack->setCurrentIndex(0);  //恢复“开始翻译”按钮  //翻译过程结束有三个情况：1、翻译完成；2、错误执行回调；3、手动中止

        //将最终文本再渲染一次，展示纯净结果，抹除残留的 "[正在翻译...]" 提示
        targetText->setPlainText(m_accumulatedResult.trimmed());

        //保存到历史记录中
        saveToHistory(sourceText->toPlainText(), langCombo->currentText(), m_accumulatedResult);
        return;
    }

    //弹出当前要翻译的块
    //获取当前块内容
    QString currentText = m_chunkList.takeFirst();
    //计算当前块ID
    int currentId = m_totalChunks - m_chunkList.size();

    //每次开始新块时，清空当前块的流式缓存
    m_currentChunkResult.clear();

    //更新UI反馈
    // //UI初始占位符（后面流式输出还会更新UI反馈）
    // targetText->setPlainText(m_accumulatedResult + QString("\n\n[正在翻译第 %1/%2 块...]\n").arg(currentId).arg(m_totalChunks));
    // //自动滚动到底部，方便查看翻译进度
    // targetText->moveCursor(QTextCursor::End);

    //配置Agent
    //(注意：这里的配置，可以clearHistory防止旧块干扰新块，也可以在提示词里告诉AI这是一个连续的文本，也可以同时使用）
    m_agent->setSystemPrompt(promptEdit->toPlainText() + "\n\n【必须翻译为】：" + langCombo->currentText() + "\n备注：这是长文的一部分，请保持前后术语一致。严禁因为看到分隔符而停止翻译！");
    m_agent->clearHistory();
    m_agent->sendMsg(currentText);
}

///
/// \brief TranslationPage::onPartialTranslationResult
/// \brief 流式增量文本渲染
/// \param text
///
void TranslationPage::onPartialTranslationResult(const QString &text)
{
    if(!m_isProcessing) return;

    //累积当前块的流式增量
    m_currentChunkResult += text;

    //计算当前是第几块
    int currentId = m_totalChunks - m_chunkList.size();

    //拼接输出文本：以前完成的所有块 + 当前块的进度提示 + 当前块正在生成的文本
    QString displayText = m_accumulatedResult +
                          QString("\n\n[正在翻译第 %1/%2 块...]\n").arg(currentId).arg(m_totalChunks) +
                          m_currentChunkResult;
    //更新UI反馈
    targetText->setPlainText(displayText);
    targetText->moveCursor(QTextCursor::End);  //自动滚动到底部
}

///
/// \brief TranslationPage::onChunkTranslationResult
/// \brief 块文本结束回调
/// \param result
///
void TranslationPage::onChunkTranslationResult(const QString &result) {
    if(!m_isProcessing) return;

    //累加结果
    m_accumulatedResult += result + "\n";
    //递归处理下一块
    processNextChunk();
}

void TranslationPage::onTranslationError(const QString &error) {
    m_isProcessing = false;
    btnStack->setCurrentIndex(0); //恢复“开始翻译”按钮

    targetText->setPlainText("错误: " + error);
    translateBtn->setEnabled(true);
}
// ********************************

void TranslationPage::exportToTxt() {
    QString content = targetText->toPlainText();
    if(content.isEmpty()) return;
    QString path = QFileDialog::getSaveFileName(this, "导出翻译", "", "Text Files (*.txt)");
    if(!path.isEmpty()) {
        QFile f(path);
        if(f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&f);
            out << content;
            f.close();
        }
    }
}

// 预设管理逻辑
void TranslationPage::refreshPresetList() {
    QSettings settings("Yachi", "PersistentData");
    QString current = presetCombo->currentText();
    settings.beginGroup("Trans_PromptPresets");
    QStringList keys = settings.allKeys();
    settings.endGroup();
    presetCombo->blockSignals(true);
    presetCombo->clear();
    presetCombo->addItems(keys);
    if (keys.contains(current)) presetCombo->setCurrentText(current);
    else if (!keys.isEmpty()) presetCombo->setCurrentIndex(0);
    presetCombo->blockSignals(false);
    loadSelectedPrompt();
}

void TranslationPage::loadSelectedPrompt() {
    QString name = presetCombo->currentText();
    if (name.isEmpty()) { promptEdit->clear(); return; }
    QSettings settings("Yachi", "PersistentData");
    promptEdit->setPlainText(settings.value("Trans_PromptPresets/" + name).toString());
}

void TranslationPage::addNewPreset() {
    QSettings settings("Yachi", "PersistentData");
    settings.beginGroup("Trans_PromptPresets");
    QStringList keys = settings.allKeys();
    settings.endGroup();
    int count = 1;
    QString newName;
    while(true) {
        newName = QString("自定义预设 %1").arg(count++);
        if (!keys.contains(newName)) break;
    }
    settings.setValue("Trans_PromptPresets/" + newName, "");  // 新预设的默认内容可以在这添加
    refreshPresetList();
    presetCombo->setCurrentText(newName);
}

void TranslationPage::renamePreset() {
    QString oldName = presetCombo->currentText();
    if (oldName.isEmpty()) return;
    bool ok;
    QString newName = QInputDialog::getText(this, "重命名", "输入新名称:", QLineEdit::Normal, oldName, &ok);
    if (ok && !newName.isEmpty()) {
        QSettings settings("Yachi", "PersistentData");
        QString val = settings.value("Trans_PromptPresets/" + oldName).toString();
        settings.remove("Trans_PromptPresets/" + oldName);
        settings.setValue("Trans_PromptPresets/" + newName, val);
        refreshPresetList();
        presetCombo->setCurrentText(newName);
    }
}

void TranslationPage::deletePreset() {
    QString name = presetCombo->currentText();
    if (name.isEmpty()) return;
    if (QMessageBox::question(this, "删除", "确认删除该预设？") == QMessageBox::Yes) {
        QSettings settings("Yachi", "PersistentData");
        settings.remove("Trans_PromptPresets/" + name);
        refreshPresetList();
    }
}

void TranslationPage::saveCurrentPrompt() {
    QString name = presetCombo->currentText();
    if (name.isEmpty()) return;
    QSettings settings("Yachi", "PersistentData");
    settings.setValue("Trans_PromptPresets/" + name, promptEdit->toPlainText());
    QMessageBox::information(this, "成功", "翻译预设已保存。");
}

// **************** 网页读取接口 ****************
void TranslationPage::fetchUrlContent()
{
    // 1.处理输入的URL
    QString rawUrl = urlEdit->text().trimmed();
    if(rawUrl.isEmpty())return;

    if(!rawUrl.startsWith("http"))
    {
        QMessageBox::warning(this, "格式错误", "请输入正确的网址（需包含 http 或 https）");
        return;
    }

    // 2.向 网页读取策略的 策略工厂 要一个策略
    auto strategy = StrategyFactory::getStrategy(rawUrl);
    QString baseUrl = strategy->extractBaseUrl(rawUrl);

    // 3.处理过程中的UI反馈
    fetchBtn->setEnabled(false);
    fetchBtn->setText("抓取中...");
    //清空旧文本
    sourceText->clear();
    //动态显示专属进度提示
    sourceText->setPlaceholderText(strategy->getLoadingTip());

    // 4.构建共享状态，用于跨网络请求累加文本和记录总页数
    struct FetchState
    {
        QString fullContent;
        int maxPage = 1;
        QStringList seriesIds;  //系列链接会有的章节ID队列
    };
    auto state = std::make_shared<FetchState>();

    // 5.定义结束操作
    auto finishFetching = [this](QString finalContent)
    {
        //恢复UI
        fetchBtn->setEnabled(true);
        fetchBtn->setText("抓取并导入");
        sourceText->setPlaceholderText("请输入源文本...");

        if(!finalContent.isEmpty())
        {
            //硬过滤 / 一级智能过滤
            if(m_hardFilterEnabled)
            {
                finalContent = applyHardFilter(finalContent);
            }

            //输出最终内容
            sourceText->setPlainText(finalContent);
            //光标自动移动到最上面
            sourceText->moveCursor(QTextCursor::Start);
        }
    };

    // 6.递归网络请求
    auto fetchNextPage = std::make_shared<std::function<void(int)>>();

    *fetchNextPage = [this, baseUrl, strategy, state, fetchNextPage, finishFetching](int currentPage) {

        QString finalRequestUrl;

        //通用策略：使用Jina Reader抓取
        if (strategy->useJina()) {
            finalRequestUrl = "https://r.jina.ai/" + strategy->buildTargetUrl(baseUrl, currentPage);
        }
        else
        {
            if(strategy->isSeries())
            {
                //系列抓取逻辑：0 阶段抓取目录，1~N 阶段抓取各个章节ID
                if (currentPage == 0) {
                    finalRequestUrl = strategy->buildTargetUrl(baseUrl, 1);
                } else {
                    //增加一层越界保护
                    if (currentPage - 1 < state->seriesIds.size()) {
                        finalRequestUrl = QString("https://www.pixiv.net/ajax/novel/%1").arg(state->seriesIds[currentPage - 1]);
                    } else {
                        return; //越界直接中止
                    }
                }
            }
            else
            {
                finalRequestUrl = strategy->buildTargetUrl(baseUrl, currentPage);
            }
        }

        //设置网络头
        QNetworkRequest request((QUrl(finalRequestUrl)));
        //设置一些基础Header模拟浏览器，防止被某些简单策略拦截
        request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) YachiAgent/1.0");

        //没有使用Jina时（如Pixiv特化策略）注入Referer绕过403限制
        if (!strategy->useJina()) {
            request.setRawHeader("Referer", "https://www.pixiv.net/");
        }

        QNetworkReply *reply = m_urlManager->get(request);

        //信号连接
        connect(reply, &QNetworkReply::finished, this, [=]() {
            if (reply->error() == QNetworkReply::NoError) {
                QString pageContent = reply->readAll();

                // --- 系列读取逻辑 ---
                if (strategy->isSeries()) {
                    if (currentPage == 0) {
                        //解析第一步获取的目录内容
                        state->seriesIds = strategy->extractSeriesNovels(pageContent);
                        state->maxPage = state->seriesIds.size();

                        if (state->maxPage == 0) {
                            QMessageBox::warning(this, "抓取失败", "未能从系列链接中提取到任何章节！（可能无权访问或链接有误）");
                            finishFetching("");
                            reply->deleteLater();
                            return;
                        }

                        sourceText->setPlaceholderText(QString("获取系列目录成功，共 %1 章。开始抓取第 1 章...").arg(state->maxPage));

                        //间隔0.5秒后开始拉取第1章正文
                        QTimer::singleShot(500, this, [=]() {
                            (*fetchNextPage)(1);
                        });
                    } else {
                        //累加各个章节正文内容
                        state->fullContent += QString("\n\n### --- 第 %1 章 ---\n\n").arg(currentPage);
                        state->fullContent += strategy->processRawContent(pageContent);

                        if (currentPage < state->maxPage) {
                            sourceText->setPlaceholderText(QString("正在自动抓取系列章节：第 %1/%2 章，请稍候...")
                                                               .arg(currentPage + 1).arg(state->maxPage));
                            //给服务器喘口气，延时1秒
                            QTimer::singleShot(1000, this, [=]() {
                                (*fetchNextPage)(currentPage + 1);
                            });
                        } else {
                            finishFetching(state->fullContent); //所有章节全部完成
                        }
                    }
                }
                // --- 单页/分页读取逻辑 ---
                else {
                    if (currentPage == 1) {
                        state->maxPage = strategy->parseMaxPage(pageContent);
                    }

                    if (state->maxPage > 1) {
                        state->fullContent += QString("\n\n### --- 第 %1 页 ---\n\n").arg(currentPage);
                    }
                    state->fullContent += strategy->processRawContent(pageContent);

                    if (currentPage < state->maxPage) {
                        sourceText->setPlaceholderText(QString("发现分页！正在自动抓取第 %1/%2 页，请稍候...")
                                                           .arg(currentPage + 1).arg(state->maxPage));

                        QTimer::singleShot(2000, this, [=]() {
                            (*fetchNextPage)(currentPage + 1);
                        });
                    } else {
                        finishFetching(state->fullContent);
                    }
                }
            }
            else { //网络异常处理
                if (currentPage <= 1) {
                    QMessageBox::critical(this, "抓取异常", QString("无法读取网页！\n错误代码：%1\n原因：%2").arg(reply->error()).arg(reply->errorString()));
                    finishFetching("");
                } else {
                    QMessageBox::warning(this, "部分抓取失败", QString("在读取阶段 %1 时发生网络错误，已中断后续抓取，将保留前面成功抓取的内容。").arg(currentPage));
                    finishFetching(state->fullContent);
                }
            }
            reply->deleteLater();
        });
    };


    //决定起点：如果是系列，从 0（目录页）开始启动；否则从 1（第一页正文）开始启动
    if (strategy->isSeries()) {
        (*fetchNextPage)(0);
    } else {
        (*fetchNextPage)(1);
    }
}

///
/// \brief TranslationPage::applyHardFilter
/// \brief （网页读取）智能过滤的硬代码过滤逻辑
/// \details 基于正则表达式
/// \param input
/// \return
///
QString TranslationPage::applyHardFilter(const QString &input)
{
    QString output = input;

    // 1.移除常见标签： ![...]、(...)、只有链接没有文字的空括号[]()
    output.remove(QRegularExpression("!?\\[\\]\\(.*?\\)"));
    output.remove(QRegularExpression("!\\[.*?\\]\\(.*?\\)"));

    // 2.移除常见的社交分析链接：针对Twitter/Ads等
    output.remove(QRegularExpression("\\[Image \\d+\\]\\(https://(analytics|t\\.co).*?\\)"));

    // 3.基于行内容的深度清理
    QStringList lines = output.split("\n");
    QStringList filteredLines;

    //定义关键字黑名单  //TODO:（这部分后续根据经验持续更新）
    QStringList blacklist = {
        "Log in", "Sign Up", "Rankings", "Post", "Help",
        "Terms of Use", "Privacy Policy", "Feedback", "Recommendation",
        "Create an account", "Related services", "Search novels",
        "Home", "Newest by all", "Requests", "Collections",
        "Contests", "Search illustrations", "Sign up to be able to Like",
        "Twitterやってるよ", "forms.gle", "View profile", "Following",
        "© pixiv",
    };

    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        bool isNoise = false;

        //如果行太短且包含黑名单关键字，或者是纯链接行
        for (const QString &word : blacklist) {
            if (trimmed.contains(word, Qt::CaseInsensitive) && trimmed.length() < 50) {
                isNoise = true;
                break;
            }
        }

        //过滤包含大量垃圾链接的行
        if (trimmed.startsWith("* [") && (trimmed.contains("login") || trimmed.contains("help"))) {
            isNoise = true;
        }

        if (!isNoise) filteredLines << line;
    }

    return filteredLines.join("\n").trimmed();
}

///
/// \brief TranslationPage::showFilterDetails
/// \brief （网页读取）智能过滤功能的UI
///
void TranslationPage::showFilterDetails()
{
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("智能过滤详情");
    dialog->setFixedWidth(500);

    //全局布局
    QVBoxLayout *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(20,20,20,20);
    layout->setSpacing(15);

    //硬代码过滤UI
    QGroupBox *hardGroup = new QGroupBox("1.硬代码过滤 / 一级过滤");
    hardGroup->setStyleSheet("QGroupBox { font-weight: bold; color: #333; border: 1px solid #ddd; border-radius: 8px; margin-top: 12px; padding-top: 15px; } "
                             "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }");

    QVBoxLayout *hLayout = new QVBoxLayout(hardGroup);
    hLayout->setSpacing(10);

    QLabel *hTip = new QLabel(R"(
        <p style='line-height: 140%;'>使用正则表达式自动剔除以下内容：</p>
        <ul style='color: #555; margin-left: -15px;'>
            <li>所有的Markdown图片标签</li>
            <li>侧边栏导航链接 (Home, Login等)</li>
            <li>Twitter广告统计代码</li>
            <li>只有超链接而没有正文的冗余行</li>
        </ul>
    )");
    hTip->setWordWrap(true);
    hTip->setStyleSheet("font-size: 13px;");

    QCheckBox *toggleBox = new QCheckBox("启用硬代码过滤");
    toggleBox->setChecked(m_hardFilterEnabled);
    toggleBox->setStyleSheet("QCheckBox { font-weight: bold; color: ##66aee5; } QCheckBox::indicator { width: 18px; height: 18px; color: #FFFFFF }");

    hLayout->addWidget(hTip);
    hLayout->addWidget(toggleBox);
    layout->addWidget(hardGroup);

    //AI过滤UI
    QGroupBox *aiGroup = new QGroupBox("2. AI过滤 / 二级过滤");
    aiGroup->setStyleSheet("QGroupBox { font-weight: bold; color: #333; border: 1px solid #ddd; border-radius: 8px; margin-top: 12px; padding-top: 15px; } "
                           "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }");

    QVBoxLayout *aLayout = new QVBoxLayout(aiGroup);
    aLayout->setSpacing(10);

    QLabel *aTip = new QLabel(R"(<p><b>如果抓取内容仍有杂质</b>，可以使用AI过滤。</p>
                              <p>
                                <b>使用方法</b>：请复制以下提示词在“翻译预设”中直接使用，或者合并到您的提示词中
                                <span style="color:grey;">（您可以调整下面的提示词以更贴合您的使用）</span>
                              </p>)");
    aTip->setWordWrap(true);

    QTextEdit *promptCopy = new QTextEdit();
    promptCopy->setPlainText("你是一个专业的网页内容提取官。接下来的输入是从网页抓取的原始文本，"
                             "请你**只识别并翻译其中的‘小说正文内容’**，忽略所有的导航菜单、"
                             "广告和社交统计链接。如果某行内容看起来像按钮或菜单，请直接丢弃。");
    promptCopy->setReadOnly(true);
    promptCopy->setFixedHeight(90);
    promptCopy->setStyleSheet(R"(
        QTextEdit {
            background-color: #f8f9fa;
            border: 1px dashed #bbb;
            border-radius: 4px;
            color: #444;
            padding: 8px;
            font-family: 'Consolas', 'Monaco', monospace;
            font-size: 12px;
        }
    )");

    //添加一个快捷复制按钮
    QPushButton *quickCopyBtn = new QPushButton("复制内容");
    quickCopyBtn->setCursor(Qt::PointingHandCursor);
    quickCopyBtn->setStyleSheet("QPushButton { color: #0078d4; border: none; text-decoration: underline; background: transparent; text-align: left; }");

    //quickCopyBtn的信号
    connect(quickCopyBtn, &QPushButton::clicked, this, [promptCopy, quickCopyBtn](){
        // 1.获取文本并写入剪贴板
        QApplication::clipboard()->setText(promptCopy->toPlainText());

        // 2.视觉反馈
        QString originalText = quickCopyBtn->text();
        //改变文本
        quickCopyBtn->setText("已复制到剪贴板 √");
        //改变样式
        quickCopyBtn->setStyleSheet("QPushButton { color: #2e7d32; border: none; font-weight: bold; background: transparent; text-align: left; }");

        // 3.使用定时器，2秒后恢复原样
        QTimer::singleShot(2000, [quickCopyBtn, originalText](){
            //增加安全检查，防止窗口在定时器触发前已关闭
            if (quickCopyBtn) {
                quickCopyBtn->setText(originalText);
                quickCopyBtn->setStyleSheet("QPushButton { color: #0078d4; border: none; text-decoration: underline; background: transparent; text-align: left; }");
            }
        });
    });

    aLayout->addWidget(aTip);
    aLayout->addWidget(promptCopy);
    aLayout->addWidget(quickCopyBtn);
    layout->addWidget(aiGroup);

    QPushButton *okBtn = new QPushButton("确定");  //确认按钮UI
    okBtn->setFixedHeight(38);
    okBtn->setCursor(Qt::PointingHandCursor);
    okBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #0078d4;
            color: white;
            border-radius: 5px;
            font-weight: bold;
            font-size: 14px;
        }
        QPushButton:hover { background-color: #005a9e; }
    )");

    layout->addWidget(okBtn);

    //确认按钮的信号
    connect(okBtn, &QPushButton::clicked, dialog, [=](){
        m_hardFilterEnabled = toggleBox->isChecked();
        //更新UI按钮显示
        if(m_hardFilterEnabled) {
            filterStatusBtn->setText("✨智能过滤设置");
            filterStatusBtn->setStyleSheet("color: #2e7d32; border: 1px solid #a5d6a7; font-weight: bold; background: #e8f5e9; padding: 2px 8px; border-radius: 4px;");
        } else {
            filterStatusBtn->setText("⚪过滤已关闭");
            filterStatusBtn->setStyleSheet("color: #757575; border: 1px solid #bdbdbd; font-weight: normal; background: #f5f5f5; padding: 2px 8px; border-radius: 4px;");
        }
        dialog->accept();
    });

    dialog->exec();
}
// ********************************

// **************** 分块翻译接口 ****************
///
/// \brief TranslationPage::splitText
/// \brief 文本分块算法
/// \param text
/// \param maxLength
/// \return
///
QStringList TranslationPage::splitText(const QString &text, int maxLength)
{
    QStringList chunks;
    QStringList paragraphs = text.split("\n"); //按行拆分
    QString currentChunk;

    for (const QString &para : paragraphs) {
        //如果当前块加上新段落超过限制，则保存当前块
        if (currentChunk.length() + para.length() > maxLength && !currentChunk.isEmpty()) {
            chunks.append(currentChunk.trimmed());
            currentChunk = para;
        } else {
            currentChunk += "\n" + para;
        }
    }

    if (!currentChunk.trimmed().isEmpty()) {
        chunks.append(currentChunk.trimmed());
    }
    return chunks;
}
// ****************

///
/// \brief TranslationPage::abortTranslation
/// \brief 中止翻译接口
///
void TranslationPage::abortTranslation()
{
    if (!m_isProcessing) return;

    // 1.核心修改：直接让Agent中断正在进行的网络传输
    m_agent->abort();

    // 2.停止分块递归
    m_isProcessing = false;
    m_chunkList.clear();

    // 3.UI反馈
    targetText->appendPlainText("\n[翻译已中断]");
    targetText->moveCursor(QTextCursor::End);

    btnStack->setCurrentIndex(0); //恢复“开始翻译”按钮
    translateBtn->setEnabled(true);
}