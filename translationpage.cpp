#include "translationpage.h"
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

TranslationPage::TranslationPage(AgentCore *agent, QWidget *parent)
    : QWidget(parent), m_agent(agent)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // 第一部分：翻译预设管理 (1:2 比例中的 1)
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

    promptEdit = new QTextEdit();
    promptEdit->setAcceptRichText(false);  // 禁止富文本，来修复拷贝内容到框里时出现带白框的问题
    promptEdit->setPlaceholderText("在这里编辑提示词...");
    promptEdit->setMinimumHeight(100);
    promptEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    configLayout->addWidget(promptEdit);

    // 第二部分：中间工具栏
    QHBoxLayout *toolBar = new QHBoxLayout();
    langCombo = new QComboBox();
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
    langCombo->setFixedWidth(110);

    exportBtn = new QPushButton("导出当前结果 (.txt)");
    historyBtn = new QPushButton("历史记录");

    toolBar->addWidget(new QLabel("<b>目标语言:</b>"));
    toolBar->addWidget(langCombo);
    toolBar->addStretch();
    toolBar->addWidget(historyBtn);
    toolBar->addWidget(exportBtn);

    // 第三部分：翻译文本区 (1:2 比例中的 2)
    QHBoxLayout *textAreaLayout = new QHBoxLayout();
    sourceText = new QTextEdit();
    sourceText->setAcceptRichText(false);  // 禁止富文本，来修复拷贝内容到框里时出现带白框的问题
    sourceText->setPlaceholderText("请输入源文本...");

    targetText = new QTextEdit();
    targetText->setReadOnly(true);
    targetText->setPlaceholderText("等待翻译结果...");
    targetText->setStyleSheet("background-color: #fcfcfc; border: 1px solid #eee;");

    translateBtn = new QPushButton("开始\n翻译");
    translateBtn->setFixedSize(75, 75);
    translateBtn->setObjectName("translateBtn");

    textAreaLayout->addWidget(sourceText, 1);
    textAreaLayout->addWidget(translateBtn);
    textAreaLayout->addWidget(targetText, 1);

    mainLayout->addWidget(configGroup, 1);
    mainLayout->addLayout(toolBar, 0);
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

    // 信号绑定
    connect(addPresetBtn, &QPushButton::clicked, this, &TranslationPage::addNewPreset);
    connect(renamePresetBtn, &QPushButton::clicked, this, &TranslationPage::renamePreset);
    connect(deletePresetBtn, &QPushButton::clicked, this, &TranslationPage::deletePreset);
    connect(savePromptBtn, &QPushButton::clicked, this, &TranslationPage::saveCurrentPrompt);
    connect(translateBtn, &QPushButton::clicked, this, &TranslationPage::doTranslate);
    connect(exportBtn, &QPushButton::clicked, this, &TranslationPage::exportToTxt);
    connect(historyBtn, &QPushButton::clicked, this, &TranslationPage::showHistory);
    connect(presetCombo, &QComboBox::currentTextChanged, this, &TranslationPage::loadSelectedPrompt);

    connect(m_agent, &AgentCore::responseMsg, this, &TranslationPage::onTranslationResult);
    connect(m_agent, &AgentCore::errorMsg, this, &TranslationPage::onTranslationError);

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
        det->setWindowTitle("翻译详情 (支持复制)");
        det->resize(550, 600);
        QVBoxLayout *v = new QVBoxLayout(det);

        v->addWidget(new QLabel(QString("<b>记录时间:</b> %1  |  <b>语种:</b> %2").arg(timeStr, langStr)));

        v->addWidget(new QLabel("<b>原文内容 (可复制):</b>"));
        QTextEdit *srcE = new QTextEdit();
        srcE->setPlainText(srcStr);
        srcE->setReadOnly(true);
        v->addWidget(srcE, 1);

        v->addWidget(new QLabel("<b>翻译结果 (可复制):</b>"));
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

void TranslationPage::doTranslate() {
    QString text = sourceText->toPlainText();
    if(text.isEmpty()) return;

    m_lastSourceText = text;
    QString prompt = QString("%1\n\n将以下文本翻译为：【%2】").arg(promptEdit->toPlainText(), langCombo->currentText());

    translateBtn->setEnabled(false);
    targetText->setPlainText("正在请求 AI...");

    m_agent->setSystemPrompt(prompt);
    m_agent->clearHistory();
    m_agent->sendMsg(text);
}

void TranslationPage::onTranslationResult(const QString &result) {
    targetText->setPlainText(result);
    translateBtn->setEnabled(true);
    saveToHistory(m_lastSourceText, langCombo->currentText(), result);
}

void TranslationPage::onTranslationError(const QString &error) {
    targetText->setPlainText("错误: " + error);
    translateBtn->setEnabled(true);
}

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