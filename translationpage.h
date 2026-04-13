#ifndef TRANSLATIONPAGE_H
#define TRANSLATIONPAGE_H

#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QGroupBox>
#include <QStackedWidget>
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

    void abortTranslation();   //中止翻译

    //网页读取
    void fetchUrlContent();    //读取网页内容的接口
    void showFilterDetails();  //显示详细过滤设置的UI

    //预设管理
    void addNewPreset();
    void renamePreset();
    void deletePreset();
    void saveCurrentPrompt();
    void refreshPresetList();
    void loadSelectedPrompt();

    //历史记录
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

    QStackedWidget *btnStack;
    QPushButton *translateBtn;
    QPushButton *stopBtn;

    QPushButton *exportBtn;
    QPushButton *historyBtn;
    QPushButton *addPresetBtn;
    QPushButton *renamePresetBtn;
    QPushButton *deletePresetBtn;
    QPushButton *savePromptBtn;

    // --- 网页读取相关 ---
    //UI
    QGroupBox *urlGroup;
    QLineEdit *urlEdit;
    QPushButton *fetchBtn;
    QLabel *urlTipLabel;
    QPushButton *filterStatusBtn; //过滤状态显示按钮

    QNetworkAccessManager *m_urlManager; //专门用于网页抓取的网络管理

    bool m_hardFilterEnabled = true;  //硬代码过滤（一级过滤）启动开关
    QString applyHardFilter(const QString &input);  //硬代码过滤接口
    // ------

    // --- 分块翻译相关 ---
    QStringList m_chunkList;      //待翻译的文本块队列
    QString m_accumulatedResult;  //累加的结果
    int m_totalChunks = 0;        //总块数，用于进度控制
    bool m_isProcessing = false;  //是否正在进行分块翻译

    //处理队列中的下一个块
    void processNextChunk();
    //文本分块算法
    QStringList splitText(const QString &text, int maxLength);
    // ------

    AgentCore *m_agent;
    QString m_lastSourceText;
};

#endif