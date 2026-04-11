#ifndef TRANSLATIONPAGE_H
#define TRANSLATIONPAGE_H

#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QGroupBox>
#include "agentcore.h"

class TranslationPage : public QWidget {
    Q_OBJECT
public:
    explicit TranslationPage(AgentCore *agent, QWidget *parent = nullptr);

private slots:
    void doTranslate();
    void onTranslationResult(const QString &result);
    void onTranslationError(const QString &error);
    void exportToTxt();
    void fetchUrlContent();  //读取网页内容的接口

    // 预设管理
    void addNewPreset();
    void renamePreset();
    void deletePreset();
    void saveCurrentPrompt();
    void refreshPresetList();
    void loadSelectedPrompt();

    // 历史记录
    void showHistory();
    void saveToHistory(const QString &source, const QString &lang, const QString &result);

private:
    void initTutorialPresets();

    //UI组件
    QTextEdit *sourceText;
    QTextEdit *targetText;
    QTextEdit *promptEdit;
    QComboBox *langCombo;
    QComboBox *presetCombo;

    QPushButton *translateBtn;
    QPushButton *exportBtn;
    QPushButton *historyBtn;
    QPushButton *addPresetBtn;
    QPushButton *renamePresetBtn;
    QPushButton *deletePresetBtn;
    QPushButton *savePromptBtn;

    // --- 读取网页相关 ---
    QGroupBox *urlGroup;
    QLineEdit *urlEdit;
    QPushButton *fetchBtn;
    QLabel *urlTipLabel;
    QNetworkAccessManager *m_urlManager; //专门用于网页抓取的网络管理

    AgentCore *m_agent;
    QString m_lastSourceText;
};

#endif