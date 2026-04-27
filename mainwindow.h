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
#include "voicepage.h"

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
    void setupUI();

    //导航栏
    QListWidget *navigationBar;
    //导航栏的栈结构
    QStackedWidget *mainStack;

    //各一级页面
    HomePage *homePageWidget;
    ChatPage *chatPageWidget;
    TranslationPage *translationPageWidget;
    QWidget *settingsPage;
    VoicePage *voicePageWidget;

    //为不同功能分配独立的AgentCore
    AgentCore *m_chatAgent;      //对话普通模式的Agent
    AgentCore *m_barAgent;       //对话酒吧模式的Agent
    AgentCore *m_translateAgent;
    AgentCore *m_titleAgent;     //对话标题生成Agent

    //厂商映射表
    QMap<QString, ModelInfo> m_vendorMap;
    void loadVendorMapFromJson();  //vendormap.json
};

#endif // MAINWINDOW_H