#ifndef AGENTCORE_H
#define AGENTCORE_H

#include <QObject>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include <QCoreApplication>

//能被Agent使用的模型所需的信息
struct ModelToUseInfo
{
    QString baseUrl;
    QString apiKey;
    QString model;
};

class AgentCore : public QObject
{
    Q_OBJECT
public: 
    explicit AgentCore(QObject* parent = nullptr);

    //设置模型配置
    void setModelConfig(const QList<ModelToUseInfo> &configs);
    //设置提示词
    void setSystemPrompt(const QString &prompt);

    //清空历史记忆
    void clearHistory();

    //发送消息接口
    void sendMsg(const QString &userPrompt);
    //发送网络请求逻辑
    void sendWebRequest();
    //中止当前网络请求
    void abort();

    //模型可用性检查
    void testConnection(const QString &baseUrl, const QString &apiKey, const QString &model);

    //设置当前工作目录
    void setWorkspacePath(const QString &path);

signals:
    void responseMsg(const QString &reply);        //最终完整回复
    void partialResponseMsg(const QString &text);  //流式的回复
    void errorMsg(const QString &error);
    void testFinishedMsg(bool success, const QString &msg);  //模型配置可用性检查结束的返回

private slots:
    void onReadyRead();  //处理流式增量数据
    void onFinished();   //当流传输结束时

private:
    // --- 备用链路逻辑层 ---
    //可用模型列表（主模型+备用链路）
    QList<ModelToUseInfo> m_modelConfigList;
    //当前模型索引
    int m_currentConfigIndex = 0;
    //根据索引设置模型配置
    void setModelConfigWithIndex(const int &index);
    // ------

    QString m_streamingBuffer;  //流式内容暂存区

    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_currentReply = nullptr;  //记录当前网络请求

    QString m_baseUrl;
    QString m_apiKey;
    QString m_model;
    QString m_systemPrompt;
    QJsonArray m_history;

    // --- 压缩历史记录 ---
    //检测并压缩历史记录
    void TriggerHistoryCompact();
    //总结历史记录的接口
    void requestSummary(const QJsonArray &toSummarize);

    int m_historyThreshold= 10;  //触发压缩的历史记录条数
    int m_historyKept = 4;       //压缩后保留的历史记录条数
    bool m_isSummarizing = false;
    /* 以这里的阈值10、和最近保留条数4为例，说明压缩后的m_history中会有多少个成员？
     * 保留最近的4条原始历史记录；
     * 插入1条压缩过的历史记录；
     * sendMsg时会加入1条用户的输入；
     * 因为压缩是在sendMsg里自动检测并执行的，所以一般还会加1条sendMsg的返回结果，即AI的回复；
     * 4（保留的上下文）+1（历史总结）+1（当前提问）+1（当前回答） = 7
     */
    // ------

    // --- 系统提示词、及持久化记忆 ---
    //读取持久化记忆文件
    QString getYachiMemory();
    //获取系统提示词
    QString getSystemPrompt();

    //当前工作目录
    QString m_workspacePath;
    // ------
};

#endif // AGENTCORE_H