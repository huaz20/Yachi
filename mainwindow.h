#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QListWidget>
#include <QMap>

#include "homepage.h"
#include "chatPage.h"
#include "agentcore.h"
#include "translationpage.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onNavigationChanged(int index);
    void handleSendRequest(const QString &text);

private:
    void setupUi();

    QListWidget *navigationBar;
    QStackedWidget *mainStack;

    HomePage *homePageWidget;
    ChatPage *chatPageWidget;
    TranslationPage *translationPageWidget;
    QWidget *settingsPage;

    // 为不同功能分配独立的AgentCore
    AgentCore *m_chatAgent;
    AgentCore *m_translateAgent;
    AgentCore *m_titleAgent; // 专职生成对话标题的 Agent

    const QMap<QString, ModelInfo> VENDOR_MAP = {
        {"Anthropic", {{"claude-3-5-sonnet-20241022", "claude-3-opus-20240229"}, "https://api.anthropic.com/v1/"}},
        {"OpenAI", {{"gpt-4o", "gpt-4-turbo", "gpt-3.5-turbo"}, "https://api.openai.com/v1/"}},
        {"Ollama 本地模型", {{"qwen2.5", "llama3.1", "deepseek-coder"}, "http://localhost:11434/v1"}},
        {"deepseek", {{"deepseek-chat", "deepseek-coder"}, "https://api.deepseek.com/v1"}}
    };
};

#endif // MAINWINDOW_H