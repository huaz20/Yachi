#include "voiceagentcore.h"
#include <QJsonDocument>
#include <QUrlQuery>
#include <QCryptographicHash>

VoiceAgentCore::VoiceAgentCore(QObject* parent) : QObject(parent) {
    //初始化网络访问管理器（它是异步工作的）
    m_networkManager = new QNetworkAccessManager(this);
}

///
/// \brief VoiceAgentCore::generate
/// \brief 发送合成请求
/// \details 流程：MD5 缓存校验 -> 构建请求 -> 异步网络发送
/// \param config
///
void VoiceAgentCore::generate(const VoiceConfig &config) {
    // 1.检查缓存
    //如果开启了缓存，先在本地找是否合成过相同的内容
    if (config.useCache && !config.cacheDir.isEmpty()) {
        QString key = generateCacheKey(config);
        QFile cacheFile(config.cacheDir + "/" + key + ".wav");
        if (cacheFile.exists() && cacheFile.open(QIODevice::ReadOnly)) {
            //命中缓存：直接发射信号并退出
            emit voiceGenerated(cacheFile.readAll());
            cacheFile.close();
            return;
        }
    }

    //通知 UI 层将“开始合成”按钮禁用并显示进度条
    emit taskStarted();

    // 2.构建 GPT-SoVITS 标准 API 请求
    QUrl url(m_serverUrl + "/tts");
    QUrlQuery query;
    query.addQueryItem("text", config.text);
    query.addQueryItem("text_lang", config.lang);
    query.addQueryItem("ref_audio_path", config.refAudioPath);
    query.addQueryItem("prompt_text", config.promptText);
    query.addQueryItem("prompt_lang", config.promptLang);
    query.addQueryItem("top_k", QString::number(config.topK));
    query.addQueryItem("top_p", QString::number(config.topP));
    query.addQueryItem("temp", QString::number(config.temp));
    query.addQueryItem("speed_factor", QString::number(config.speed));

    url.setQuery(query);

    // 3.发送异步请求
    QNetworkRequest request(url);
    QNetworkReply *reply = m_networkManager->get(request);

    //结束回调，成功与否都会发送
    connect(reply, &QNetworkReply::finished, this, &VoiceAgentCore::onReplyFinished);
}

void VoiceAgentCore::onReplyFinished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        //读取服务器返回的二进制音频流（通常是 PCM 或 WAV）
        QByteArray data = reply->readAll();
        //此处可增加写入缓存的逻辑
        emit voiceGenerated(data);
        //TODO:缓存机制之后在这里扩展下
    } else {
        emit errorOccurred(QString("语音合成失败: %1").arg(reply->errorString()));
    }
    reply->deleteLater();
}

///
/// \brief VoiceAgentCore::generateCacheKey
/// \brief 生成唯一的缓存文件名
/// \details 将文本内容 + 部分参数进行MD5运算，确保“内容变则文件名”变
/// \param config
/// \return
///
QString VoiceAgentCore::generateCacheKey(const VoiceConfig &config) {
    //根据文本和部分参数生成唯一标识
    QString raw = config.text + config.refAudioPath + QString::number(config.speed);
    return QCryptographicHash::hash(raw.toUtf8(), QCryptographicHash::Md5).toHex();
}