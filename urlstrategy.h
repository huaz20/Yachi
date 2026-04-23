#ifndef URLSTRATEGY_H
#define URLSTRATEGY_H

#include <QString>
#include <QRegularExpression>
#include <QStringList>
#include <memory>
#include <QJsonDocument>
#include <QJsonObject>

// ==========================================
// 网页读取策略基类 (接口)
// =========================================
class IUrlStrategy
{
public:
    virtual ~IUrlStrategy() = default;

    // 1.是否匹配，匹配则调用本策略
    virtual bool canHandle(const QString &url) const = 0;

    // 2.提取基准网址（剔除页码等干扰后缀）
    virtual QString extractBaseUrl(const QString &url) const
    {
        return url;
    }

    // 3.构造目标网址
    virtual QString buildTargetUrl(const QString &baseUrl, int page) const
    {
        Q_UNUSED(page);

        return baseUrl;  //默认没有处理，直接返回baseUrl
    }

    // 4.是否使用Jina Reader
    virtual bool useJina() const { return true; }  //默认使用

    //解析总页数接口
    virtual int parseMaxPage(const QString &pageContent) const
    {
        Q_UNUSED(pageContent);
        return 1;  //默认只有1页
    }

    //网页读取进度反馈
    virtual QString getLoadingTip() const {
        return "正在通过 Jina Reader 提取网页正文，请稍候...";
    }

    //对返回内容进行一次处理的接口
    virtual QString processRawContent(const QString &rawContent) const
    {
        return rawContent;
    }
};


// ==========================================
// 默认策略（通用）
// ==========================================
class DefaultStrategy : public IUrlStrategy
{
public:
    bool canHandle(const QString &url) const override
    {
        Q_UNUSED(url);
        return true;     //作为最后兜底的策略，永远返回true
    }
};

// ==========================================
// Pixiv特化策略
// ==========================================
class PixivStrategy : public IUrlStrategy
{
public:
    bool useJina() const override { return false; }  //因为可以读接口，这里选择不用Jina中转

    bool canHandle(const QString &url) const override
    {
        return url.contains("pixiv.net/novel/show.php?id=");
    }

    ///
    /// \brief extractBaseUrl
    /// \brief 处理成Pixiv AJAX接口的URL
    /// \details Pixiv隐藏返回接口：https://www.pixiv.net/ajax/novel/{id}
    /// \param url
    /// \return Pixiv AJAX接口的URL
    ///
    QString extractBaseUrl(const QString &url) const override
    {
        //从原网址中提取出小说 ID，转换为Pixiv内部的 AJAX 接口
        QRegularExpression re("id=(\\d+)");
        QRegularExpressionMatch match = re.match(url);
        if (match.hasMatch()) {
            QString id = match.captured(1);
            return QString("https://www.pixiv.net/ajax/novel/%1").arg(id);
        }
        return url;
    }

    ///
    /// \brief buildTargetUrl
    /// \brief 构造Pixiv风格的目标地址（Pixiv通过前端路由，每次小说翻页时会在尾部加#2、#3来表示页数）
    /// \param baseUrl
    /// \param page
    /// \return
    ///
    QString buildTargetUrl(const QString &baseUrl, int page) const override
    {
        Q_UNUSED(page);
        return baseUrl;
    }

    int parseMaxPage(const QString &pageContent) const override
    {
        Q_UNUSED(pageContent);
        return 1;  //读取接口，只需读一次就可以获取到全文
    }

    QString getLoadingTip() const override
    {
        return "检测到 Pixiv 小说，正在通过内部接口一键秒抓全文...";
    }

    QString processRawContent(const QString &rawContent) const override {
        QString jsonStr = rawContent.trimmed();

        //提取纯Json内容
        //如果被包含在Markdown代码块里，进行提取
        if (jsonStr.startsWith("```")) {
            int startIndex = jsonStr.indexOf('{');
            int endIndex = jsonStr.lastIndexOf('}');
            if (startIndex != -1 && endIndex != -1) {
                jsonStr = jsonStr.mid(startIndex, endIndex - startIndex + 1);
            }
        }

        //解析接口返回的JSON
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        if (doc.isObject()) {
            QJsonObject root = doc.object();

            //【情况 A】成功获取到小说正文
            if (root.contains("body") && root["body"].isObject()) {
                QJsonObject body = root["body"].toObject();

                QString title = body["title"].toString();
                QString author = body["userName"].toString();

                //提取简介、用回车替换 <br />
                QString description = body["description"].toString();
                description.replace(QRegularExpression("<br\\s*/?>"), "\n");

                //提取正文、用纯文本分页符替换 [newpage]
                QString content = body["content"].toString();
                content.replace("[newpage]", "\n\n"
                                             "                       ◆ ◆ ◆\n"
                                             "                      （下一页）\n"
                                             "                       ◆ ◆ ◆\n\n");

                //组装成一个纯文本排版
                return QString("《%1》\n"
                               "作者：%2\n\n"
                               "【内容简介】\n"
                               "%3\n\n"
                               "============================================================\n\n"
                               "%4\n\n"
                               "============================================================")
                    .arg(title, author, description, content);
            }
            //【情况 B】因为某些报错返回的是JSON，而不是小说正文（利用Qt的 Indented 自动将 \uXXXX 转回中文并排版）
            return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
        }
        //【情况 C】兜底处理：手动解码原始文本中的 \uXXXX
        return decodeUnicodeEscapes(jsonStr);
    }

private:
    ///
    /// \brief decodeUnicodeEscapes
    /// \brief 工具函数，硬代码将字符串中的 \uXXXX 替换为正常字符
    /// \param input
    /// \return
    ///
    QString decodeUnicodeEscapes(const QString &input) const {
        QString result = input;
        QRegularExpression rx("\\\\u([0-9a-fA-F]{4})");
        QRegularExpressionMatch match;
        int offset = 0;

        while ((match = rx.match(result, offset)).hasMatch()) {
            QString hexStr = match.captured(1);
            QChar ch(hexStr.toUShort(nullptr, 16));
            result.replace(match.capturedStart(), 6, QString(ch));
            offset = match.capturedStart() + 1;
        }
        return result;
    }
};

// ==========================================
// 策略工厂（根据输入的 URL 自动分发任务）
// ==========================================
class StrategyFactory
{
public:
    static std::shared_ptr<IUrlStrategy> getStrategy(const QString &url)
    {
        PixivStrategy pixiv;
        if(pixiv.canHandle(url)) return std::make_shared<PixivStrategy>();

        //未来在这里还可以加其他网站的特化策略。
        // TwitterStrategy twitter;
        // if (twitter.canHandle(url)) return std::make_shared<TwitterStrategy>();

        //没有任何匹配，使用兜底的普通网页策略
        return std::make_shared<DefaultStrategy>();
    }
};

#endif // URLSTRATEGY_H
