#include "mainwindow.h"
#include <QMessageBox>
#include <QTimer>
#include <QHBoxLayout>
#include <QFile>
#include <QJsonDocument>
#include <QProgressDialog>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    //加载需要的Json文件
    loadVendorMapFromJson();

    // --- 初始化各功能Agent---
    //实例（不共用1个，以防上下文串联）
    m_chatAgent = new AgentCore(this);
    m_chatAgent->setSystemPrompt("和用户进行有用的、友善的聊天吧。");

    m_translateAgent = new AgentCore(this);

    m_titleAgent = new AgentCore(this);
    m_titleAgent->setSystemPrompt("你是一个标题生成助手，请根据用户的输入，用10个字以内的短语概括该对话的主题，禁止回复其他任何客套话，禁止加标点符号！");

    //功能配置
    //显式设置工作目录
    QString path = QDir(QCoreApplication::applicationDirPath()).filePath("sys");

    m_chatAgent->setWorkspacePath(path);
    m_chatAgent->setYachiMemoryEnabled(true);  //开启持久化记忆
    m_chatAgent->setEnvSenseEnabled(true);     //开启环境感知

    m_translateAgent->setWorkspacePath(path);
    m_translateAgent->setYachiMemoryEnabled(false);  //关闭持久化记忆
    m_translateAgent->setEnvSenseEnabled(false);     //关闭环境感知

    m_titleAgent->setWorkspacePath(path);
    m_titleAgent->setYachiMemoryEnabled(false);  //关闭持久化记忆
    m_titleAgent->setEnvSenseEnabled(false);     //关闭环境感知
    // ------

    //UI初始化
    setupUI();

    // --- Agent连接 ---
    connect(m_chatAgent, &AgentCore::partialResponseMsg, chatPageWidget, &ChatPage::handleStreamingResponse);
    connect(m_chatAgent, &AgentCore::responseMsg, this, [this](const QString &msg){
        chatPageWidget->finishStreamingResponse(msg);
    });
    connect(m_chatAgent, &AgentCore::errorMsg, this, [this](const QString &err){
        chatPageWidget->appendSystemMsg(err);
    });
    //更新对话标题
    connect(m_titleAgent, &AgentCore::responseMsg, this, [this](const QString &titleText){
        chatPageWidget->updateCurrentChatTitle(titleText);
    });
    // -------

    //持久化配置定时器（存储在注册表regedit里）
    QTimer::singleShot(100, this, [this](){
        auto configs = homePageWidget->getAllConfigs();
        if(!configs.isEmpty() && !configs[0].baseUrl.isEmpty()){
            QList<ModelToUseInfo> modelConfigs;
            for(const auto& c : configs) {
                modelConfigs.append({c.baseUrl, c.apiKey, c.model});
            }

            m_chatAgent->setModelConfig(modelConfigs);
            m_translateAgent->setModelConfig(modelConfigs);
            m_titleAgent->setModelConfig(modelConfigs);
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
    navigationBar->addItems({"首页配置", "聊天助手", "翻译助手", "语音生成", "设置"});
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
    homePageWidget = new HomePage(m_vendorMap, this);
    chatPageWidget = new ChatPage(this);
    translationPageWidget = new TranslationPage(m_translateAgent, this);
    settingsPage = new QWidget(this);
    voicePageWidget = new VoicePage(this);

    mainStack->addWidget(homePageWidget);
    mainStack->addWidget(chatPageWidget);
    mainStack->addWidget(translationPageWidget);
    mainStack->addWidget(voicePageWidget);
    mainStack->addWidget(settingsPage);

    mainLayout->addWidget(navigationBar);
    mainLayout->addWidget(mainStack);
    setCentralWidget(centralWidget);

    //设置主窗口大小
    this->resize(1120, 840);

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
        if(configs.isEmpty()) return;

        //UI层中读取到的模型信息数据结构 -> 逻辑层需要的模型信息数据结构
        QList<ModelToUseInfo> modelConfigs;
        for(const auto& c : configs) {
            modelConfigs.append({c.baseUrl, c.apiKey, c.model});
        }

        // --- 模型可用性验证 ---
        //UI反馈
        QProgressDialog *pd = new QProgressDialog("正在全链路验证模型...", "取消", 0, modelConfigs.size(), this);
        pd->setWindowModality(Qt::WindowModal);
        pd->show();

        //定义一个递归验证的 Lambda 闭包
        //使用 std::function 包装以便在 Lambda 内部引用自身
        auto validateNext = std::make_shared<std::function<void(int)>>();

        *validateNext = [this, pd, modelConfigs, validateNext](int index) {
            if (pd->wasCanceled()) return;  //用户取消

            //UI反馈
            pd->setValue(index);
            pd->setLabelText(QString("正在验证模型 #%1...").arg(index + 1));

            //断开旧连接，防止干扰
            disconnect(m_chatAgent, &AgentCore::testFinishedMsg, nullptr, nullptr);

            //绑定单次验证结果
            connect(m_chatAgent, &AgentCore::testFinishedMsg, this, [=](bool success, const QString &msg) {
                if (success) {
                    if (index + 1 < modelConfigs.size()) {
                        //验证下一个
                        (*validateNext)(index + 1);
                    } else {
                        //全部验证通过！
                        pd->close();
                        m_chatAgent->setModelConfig(modelConfigs);
                        m_translateAgent->setModelConfig(modelConfigs);
                        m_titleAgent->setModelConfig(modelConfigs);
                        QMessageBox::information(this, "成功", "全链路验证通过，配置已生效！");
                    }
                } else {
                    //其中一个失败
                    pd->close();
                    QMessageBox::critical(this, "验证失败", QString("模型 #%1 (%2) 验证失败：\n\n%3")
                                                                .arg(index + 1).arg(modelConfigs[index].model).arg(msg));
                }
            });

            //发起当前索引的测试
            m_chatAgent->testConnection(modelConfigs[index].baseUrl,
                                        modelConfigs[index].apiKey,
                                        modelConfigs[index].model);
        };

        // 从第 0 个开始启动
        (*validateNext)(0);
        // ------
    });

    //多会话机制的UI响应逻辑
    connect(chatPageWidget, &ChatPage::chatSessionChanged, this, [this](const QString &sessionId){
        // 1.让AgentCore切换上下文
        m_chatAgent->switchSession(sessionId);

        // 2.获取切换后的历史记录
        QJsonArray history = m_chatAgent->getSessionHistory(sessionId);

        // 3.通知UI重绘所有气泡
        chatPageWidget->rebuildChatFromHistory(history);
    });
}


void MainWindow::loadVendorMapFromJson()
{
    QFile file(":/vendormap.json");

    // TODO:回头这些报错也在客户端某个位置反馈一下
    if(!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "无法打开模型配置文件，使用空配置";
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if(!doc.isObject()) return;

    QJsonObject root = doc.object();
    QJsonArray vendors = root["vendors"].toArray();

    m_vendorMap.clear();
    for (int i = 0; i < vendors.size(); ++i) {
        QJsonObject v = vendors[i].toObject();
        QString name = v["name"].toString();
        QString baseUrl = v["baseUrl"].toString();
        QJsonArray modelArray = v["models"].toArray();

        QStringList models;
        for (auto m : modelArray) models << m.toString();

        m_vendorMap.insert(name, {models, baseUrl});
    }
}