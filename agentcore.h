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

signals:
    void responseMsg(const QString &reply); //AI的回答
    void errorMsg(const QString &error);
    void testFinishedMsg(bool success, const QString &msg);  //模型配置可用性检查结束的返回

private slots:
    void onFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_networkManager;
    QString m_baseUrl;
    QString m_apiKey;
    QString m_model;
    QString m_systemPrompt;
    QJsonArray m_history;
};

#endif // AGENTCORE_H