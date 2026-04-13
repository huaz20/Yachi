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
    if(userPrompt.trimmed().isEmpty())return;

    // 1.检查是否需要压缩历史记录
    TriggerHistoryCompact();

    // 2.记录当前用户消息
    QJsonObject userMsg;
    userMsg.insert("role", "user");
    userMsg.insert("content", userPrompt);
    m_history.append(userMsg);

    // 3.构建完整的请求（模型信息 + System Prompt + History）
    //模型信息
    QJsonObject root;
    root.insert("model", m_model.isEmpty() ? "llama3" : m_model);

    QJsonArray messagesToSend;
    if (!m_systemPrompt.isEmpty()) {
        QJsonObject sysMsg;                 //System Prompt
        sysMsg.insert("role", "system");
        sysMsg.insert("content", m_systemPrompt);
        messagesToSend.append(sysMsg);
    }
    for(auto val : m_history) {
        messagesToSend.append(val);         //History
    }
    root.insert("messages", messagesToSend);
    root.insert("max_tokens",4096);  //显式要求模型输出更多内容

    // 4.发送POST请求
    QString endpoint = m_baseUrl;
    if(!endpoint.endsWith("/")) endpoint += "/";
    endpoint += "chat/completions";

    QNetworkRequest request((QUrl(endpoint)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!m_apiKey.isEmpty()) {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());
    }

    m_currentReply = m_networkManager->post(request, QJsonDocument(root).toJson());
    connect(m_currentReply, &QNetworkReply::finished, this, [this]() {
        onFinished(m_currentReply);
        m_currentReply = nullptr;  //信号处理完后置空
    });
}

void AgentCore::onFinished(QNetworkReply *reply) {
    if(!reply) return;
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
        }
        else if(reply->error() ==  QNetworkReply::OperationCanceledError)  //玩家手动中断网络请求
        {
            qDebug()<<"Network request was aborted by user.";
        }
        else
        {
            emit errorMsg(QString("[网络错误] %1").arg(reply->errorString()));
        }
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

///
/// \brief AgentCore::abort
/// \brief 中断当前的网络请求
///
void AgentCore::abort()
{
    if(m_currentReply && m_currentReply->isRunning())
    {
        m_currentReply->abort();   //调用Qt原生的中断接口
        m_currentReply = nullptr;  //请求置空
    }
}

// **************** AGENT优化逻辑 ****************
///
/// \brief AgentCore::TriggerHistoryCompact
/// \brief 判断并触发历史记录压缩
///
void AgentCore::TriggerHistoryCompact()
{
    //如果正在总结 或者 还没达到触发压缩的阈值，跳过
    if(m_isSummarizing || m_history.size() < m_historyThreshold)return;

    qDebug() << "History too long. Starting History Compaction...";

    // 1.处理需要压缩的部分
    //提取出来放进一个Json
    int numToCompact = m_history.size() - m_historyKept;
    QJsonArray toSummarize;
    for(int i = 0; i < numToCompact; ++i) {
        toSummarize.append(m_history.at(i));
    }

    //从主历史中移除这些旧消息
    for(int i = 0; i < numToCompact; ++i) {
        m_history.removeFirst();
    }

    // 2.启动异步总结
    requestSummary(toSummarize);
}

void AgentCore::requestSummary(const QJsonArray &toSummarize)
{
    m_isSummarizing = true;

    //构建用户信息
    QJsonObject summaryPrompt;
    summaryPrompt.insert("role", "system");
    summaryPrompt.insert("content", "你是一个对话压缩助手。请简要总结以下对话的历史要点、已达成的共识和待处理的任务。请保持客观，字数控制在200字以内。");

    QJsonArray messages;
    messages.append(summaryPrompt);
    for(auto m : toSummarize) messages.append(m);

    //构建完整请求
    QJsonObject root;
    root.insert("model", m_model);
    root.insert("messages", messages);
    root.insert("stream", false); //总结请求不需要流式

    //发送POST请求
    QNetworkRequest request(m_baseUrl + "/chat/completions");
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if(!m_apiKey.isEmpty()) request.setRawHeader("Authorization", "Bearer " + m_apiKey.toUtf8());

    QNetworkReply *reply = m_networkManager->post(request, QJsonDocument(root).toJson());

    //处理总结结果，并插入历史记录顶端（prepend）
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if(reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QString summary = doc.object()["choices"].toArray()[0].toObject()["message"].toObject()["content"].toString();

            //将总结作为“历史背景”插入历史记录的最前面
            QJsonObject historyContext;
            historyContext.insert("role", "system");
            historyContext.insert("content", "[之前的对话总结]: " + summary);

            m_history.prepend(historyContext);

            qDebug() << "Compaction successful. New History size:" << m_history.size();
        }
        m_isSummarizing = false;
        reply->deleteLater();
    });
}
// ********************************