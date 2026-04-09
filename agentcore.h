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

    // 动态更新配置 (替代了原来的 setApiKey)
    void setConfig(const QString &baseUrl, const QString &apiKey, const QString &model);
    // 设置角色提示词（如：扮演翻译官）
    void setSystemPrompt(const QString &prompt);
    // 清空历史记忆
    void clearHistory();
    // 发送消息
    void sendMsg(const QString &userPrompt);

signals:
    void responseMsg(const QString &reply);
    void errorMsg(const QString &error);

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