#include "mainwindow.h"
#include <QMessageBox>
#include <QTimer>
#include <QHBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // --- 初始化各功能Agent---
    //（不共用1个是为了让上下文不串联）
    m_chatAgent = new AgentCore(this);
    m_chatAgent->setSystemPrompt("和用户进行有用的、友善的聊天吧。");

    m_translateAgent = new AgentCore(this);

    m_titleAgent = new AgentCore(this);
    m_titleAgent->setSystemPrompt("你是一个标题生成助手，请根据用户的输入，用10个字以内的短语概括该对话的主题，禁止回复其他任何客套话，禁止加标点符号！");

    //UI初始化
    setupUI();

    // --- Agent连接 ---
    connect(m_chatAgent, &AgentCore::responseMsg, this, [this](const QString &msg){
        chatPageWidget->appendMessage("AI", msg, "#0078d4");
    });
    connect(m_chatAgent, &AgentCore::errorMsg, this, [this](const QString &err){
        chatPageWidget->appendSystemMsg(err);
    });
    //更新对话标题
    connect(m_titleAgent, &AgentCore::responseMsg, this, [this](const QString &titleText){
        chatPageWidget->updateCurrentChatTitle(titleText);
    });

    //持久化配置定时器（存储在注册表regedit里）
    QTimer::singleShot(100, this, [this](){
        auto configs = homePageWidget->getAllConfigs();
        if(!configs.isEmpty() && !configs[0].baseUrl.isEmpty()){
            m_chatAgent->setConfig(configs[0].baseUrl, configs[0].apiKey, configs[0].model);
            m_translateAgent->setConfig(configs[0].baseUrl, configs[0].apiKey, configs[0].model);
            m_titleAgent->setConfig(configs[0].baseUrl, configs[0].apiKey, configs[0].model);
        }
    });
}

MainWindow::~MainWindow() {}

void MainWindow::onNavigationChanged(int index) {
    mainStack->setCurrentIndex(index);
}

void MainWindow::handleSendRequest(const QString &text) {
    chatPageWidget->appendMessage("我", text, "black");
    m_chatAgent->sendMsg(text);
}

void MainWindow::setupUI()
{
    //核心UI
    QWidget *centralWidget = new QWidget(this);
    //设置全局字体及调大后的字号 (14px)
    centralWidget->setStyleSheet("QWidget { font-family: 'Microsoft YaHei', 'Segoe UI'; font-size: 14px; }");
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);

    //侧边导航栏
    navigationBar = new QListWidget(this);
    navigationBar->addItems({"首页配置", "聊天助手", "翻译助手", "设置"});
    navigationBar->setMaximumWidth(160);
    navigationBar->setStyleSheet(R"(
        QListWidget {
            border-right: 1px solid #ddd;
            background: #f8f9fa;
            font-size: 14px;
            outline: none;
        }
        QListWidget::item { height: 45px; padding-left: 10px; border-bottom: 1px solid #f0f0f0; }
        QListWidget::item:selected { background-color: #e3f2fd; color: #0078d4; border-left: 4px solid #0078d4; }
    )");

    mainStack = new QStackedWidget(this);
    homePageWidget = new HomePage(VENDOR_MAP, this);
    chatPageWidget = new ChatPage(this);
    translationPageWidget = new TranslationPage(m_translateAgent, this);
    settingsPage = new QWidget(this);

    mainStack->addWidget(homePageWidget);
    mainStack->addWidget(chatPageWidget);
    mainStack->addWidget(translationPageWidget);
    mainStack->addWidget(settingsPage);

    mainLayout->addWidget(navigationBar);
    mainLayout->addWidget(mainStack);
    setCentralWidget(centralWidget);

    //设置主窗口大小
    this->resize(1024, 768);

    //UI连接
    connect(navigationBar, &QListWidget::currentRowChanged, this, &MainWindow::onNavigationChanged);

    connect(chatPageWidget, &ChatPage::sendMessage, this, &MainWindow::handleSendRequest);

    connect(chatPageWidget, &ChatPage::requestClearHistory, this, [this](){
        m_chatAgent->clearHistory();
    });

    connect(chatPageWidget, &ChatPage::requestTitleSummary, this, [this](const QString &firstMsg){
        m_titleAgent->clearHistory();
        m_titleAgent->sendMsg("请概括以下内容作为标题：" + firstMsg);
    });

    //响应首页配置保存并应用到所有Agent
    connect(homePageWidget, &HomePage::settingsApplied, this, [this](){
        auto configs = homePageWidget->getAllConfigs();
        if(!configs.isEmpty()){
            auto c = configs[0];
            m_chatAgent->setConfig(c.baseUrl, c.apiKey, c.model);
            m_translateAgent->setConfig(c.baseUrl, c.apiKey, c.model);
            m_titleAgent->setConfig(c.baseUrl, c.apiKey, c.model);
        }
    });

}