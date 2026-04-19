#ifndef URLSTRATEGY_H
#define URLSTRATEGY_H

#include <QString>
#include <QRegularExpression>
#include <QStringList>
#include <memory>

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
    bool canHandle(const QString &url) const override
    {
        return url.contains("pixiv.net/novel/show.php?id=");
    }

    QString extractBaseUrl(const QString &url) const override
    {
        return url.split("#").first();
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
        if(page>1)
        {
            //动态拼接 #2, #3 等后缀
            return QString("%1#%2").arg(baseUrl).arg(page);
        }
        return baseUrl;
    }

    int parseMaxPage(const QString &pageContent) const override
    {
        QRegularExpression re("Next\\s+([\\d\\s]+)\\s+Like", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch match = re.match(pageContent);
        if (match.hasMatch()) {
            QString nums = match.captured(1).trimmed();
            QStringList numList = nums.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (!numList.isEmpty()) {
                return numList.last().toInt();
            }
        }
        return 1;
    }

    QString getLoadingTip() const override
    {
        return "检测到Pixiv小说，正在读取第一页并分析总页数...";
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
