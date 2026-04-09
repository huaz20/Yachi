#include "agentcore.h"
#include <QtNetwork/QNetworkRequest>
#include <QJsonDocument>

AgentCore::AgentCore(QObject *parent) : QObject(parent) {
    m_networkManager = new QNetworkAccessManager(this);
}

void AgentCore::setConfig(const QString &baseUrl, const QString &apiKey, const QString &model) {
    m_baseUrl = baseUrl;
    m_apiKey = apiKey;
    m_model = model;
}

void AgentCore::setSystemPrompt(const QString &prompt) {
    m_systemPrompt = prompt;
}

void AgentCore::clearHistory() {
    m_history = QJsonArray(); // 重置为空
}

void AgentCore::sendMsg(const QString &userPrompt) {
    if(m_baseUrl.isEmpty()) {
        emit errorMsg("[Error] 尚未配置 Base URL！(本地模型通常为 http://localhost:11434/v1)");
        return;
    }

    QJsonObject userMsg;
    userMsg.insert("role", "user");
    userMsg.insert("content", userPrompt);
    m_history.append(userMsg);

    QJsonObject root;
    root.insert("model", m_model.isEmpty() ? "llama3" : m_model);

    QJsonArray messagesToSend;
    if (!m_systemPrompt.isEmpty()) {
        QJsonObject sysMsg;
        sysMsg.insert("role", "system");
        sysMsg.insert("content", m_systemPrompt);
        messagesToSend.append(sysMsg);
    }
    for(auto val : m_history) {
        messagesToSend.append(val);
    }
    root.insert("messages", messagesToSend);

    QString endpoint = m_baseUrl;
    if(!endpoint.endsWith("/")) endpoint += "/";
    endpoint += "chat/completions";

    QNetworkRequest request((QUrl(endpoint)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!m_apiKey.isEmpty()) {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());
    }

    QNetworkReply *reply = m_networkManager->post(request, QJsonDocument(root).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onFinished(reply);
    });
}

void AgentCore::onFinished(QNetworkReply *reply) {
    reply->deleteLater();
    if(reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument jsonResponse = QJsonDocument::fromJson(responseData);

        QJsonArray choices = jsonResponse.object()["choices"].toArray();
        if(!choices.isEmpty()) {
            QString assistantText = choices[0].toObject()["message"].toObject()["content"].toString();

            QJsonObject asstMsg;
            asstMsg.insert("role", "assistant");
            asstMsg.insert("content", assistantText);
            m_history.append(asstMsg);

            emit responseMsg(assistantText);
        } else {
            emit errorMsg("[Error] API 返回格式异常");
        }
    } else {
        emit errorMsg(QString("[Network Error] %1").arg(reply->errorString()));
    }
}