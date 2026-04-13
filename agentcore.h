#ifndef AGENTCORE_H
#define AGENTCORE_H

#include <QObject>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QJsonArray>
#include <QJsonObject>

class AgentCore : public QObject
{
    Q_OBJECT
public: 
    explicit AgentCore(QObject* parent = nullptr);

    //动态更新配置的接口
    void setConfig(const QString &baseUrl, const QString &apiKey, const QString &model);
    //设置提示词
    void setSystemPrompt(const QString &prompt);
    //清空历史记忆
    void clearHistory();
    //发送消息
    void sendMsg(const QString &userPrompt);

    //模型可用性检查
    void testConnection(const QString &baseUrl, const QString &apiKey, const QString &model);

    //中止当前网络请求
    void abort();

signals:
    void responseMsg(const QString &reply);  //最终完整回复
    void errorMsg(const QString &error);
    void testFinishedMsg(bool success, const QString &msg);  //模型配置可用性检查结束的返回

private slots:
    void onFinished(QNetworkReply *reply);

private:
    void TriggerHistoryCompact();  //压缩历史记录的逻辑
    void requestSummary(const QJsonArray &toSummarize);  //总结Json内容的接口

    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_currentReply = nullptr;  //记录当前网络请求

    QString m_baseUrl;
    QString m_apiKey;
    QString m_model;
    QString m_systemPrompt;
    QJsonArray m_history;

    int m_historyThreshold= 10;  //触发压缩的历史记录条数
    int m_historyKept = 4;  //压缩后保留的历史记录条数
    bool m_isSummarizing = false;
    /* 以这里的阈值10、和最近保留条数4为例，说明压缩后的m_history中会有多少个成员？
     * 保留最近的4条原始历史记录；
     * 插入1条压缩过的历史记录；
     * sendMsg时会加入1条用户的输入；
     * 因为压缩是在sendMsg里自动检测并执行的，所以一般还会加1条sendMsg的返回结果，即AI的回复；
     * 4（保留的上下文）+1（历史总结）+1（当前提问）+1（当前回答） = 7
     */
};

#endif // AGENTCORE_H