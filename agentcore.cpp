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

    root.insert("max_tokens",4096); //显式要求模型输出更多内容

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


///
/// \brief AgentCore::testConnection
/// \brief 模型配置可用性的检查接口
/// \param baseUrl
/// \param apiKey
/// \param model
///
void AgentCore::testConnection(const QString &baseUrl, const QString &apiKey, const QString &model)
{
    // 1.处理baseUrl
    QString endpoint = baseUrl;
    if(!endpoint.endsWith("/")) endpoint += "/";  //如果不是'/'结尾
    endpoint += "chat/completions";

    // 2.写一个Json报文
    QJsonObject root;
    root.insert("model", model.isEmpty() ? "gpt-3.5-turbo" : model);

    QJsonArray messages;
    QJsonObject msg;
    msg.insert("role","user");  //"role" : "user"
    msg.insert("content","This is a connection test, please reply 'OK'.");  //"content" : "比较特殊的测试词，以防和用户的内容产生污染"
    messages.append(msg);
    root.insert("messages",messages);

    // 3.写网络头
    QNetworkRequest request((QUrl(endpoint)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!apiKey.isEmpty()) {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    }

    QNetworkReply *reply = m_networkManager->post(request, QJsonDocument(root).toJson());

    //连接到一个临时的处理槽
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if(reply->error() == QNetworkReply::NoError) {
            emit testFinishedMsg(true, "连接成功！");
        } else {
            //解析错误码，转换报错提示为用户容易懂的
            QString userError;
            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

            switch (statusCode) {
            case 401: userError = "API Key 无效，请检查填写的 Key 是否正确。"; break;
            case 404: userError = "Base URL 路径错误，未找到 API 接口。"; break;
            case 403: userError = "服务器拒绝访问，可能是权限不足或地区被封锁。"; break;
            case 429: userError = "频率限制：请求过于频繁，或账户额度已用完。"; break;
            default:
                if(reply->error() == QNetworkReply::ConnectionRefusedError)
                    userError = "无法连接到服务器，请检查网络或代理设置。";
                else
                    userError = QString("网络错误 (%1): %2").arg(statusCode).arg(reply->errorString());
            }
            emit testFinishedMsg(false, userError);
        }
    });
}