#ifndef VOICEAGENT_H
#define VOICEAGENT_H

#include <QObject>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QByteArray>
#include <QJsonObject>
#include <QDir>
#include <QFile>

//语音配置参数结构体
struct VoiceConfig {
    QString text;           //待合成的正文内容
    QString lang = "zh";    //正文语种
    //参考音频相关
    QString refAudioPath;
    QString promptText;
    QString promptLang = "zh";
    //模型路径
    QString gptPath;
    QString sovitsPath;
    //推理参数
    double speed = 1.0;     //语速因子
    double topP = 0.8;      //采样阈值，值越高声音越多样但可能越不稳定
    int topK = 50;          //采样范围，限制模型只从最可能的 K 个 token 中选择
    double temp = 0.7;      //采样温度，控制输出的“拟人性”
    //缓存配置
    bool useCache = true;   //是否开启本地缓存
    QString cacheDir;       //缓存文件存储目录
};

class VoiceAgentCore : public QObject {
    Q_OBJECT
public:
    explicit VoiceAgentCore(QObject* parent = nullptr);

    //发起合成请求
    void generate(const VoiceConfig &config);
    //设置服务端地址 (例如 http://127.0.0.1:9880)
    void setServerUrl(const QString &url) { m_serverUrl = url; }

signals:
    void taskStarted();                          //任务开始，主要用于通知UI层
    void voiceGenerated(const QByteArray &data); //合成成功，返回二进制音频
    void errorOccurred(const QString &msg);      //错误捕获信号

private slots:
    void onReplyFinished();  //处理服务器返回的数据

private:
    //生成唯一的缓存文件名
    QString generateCacheKey(const VoiceConfig &config);

    QNetworkAccessManager *m_networkManager;
    QString m_serverUrl = "http://127.0.0.1:9880";
};

#endif