#ifndef ANNOUNCEMENTDIALOG_H
#define ANNOUNCEMENTDIALOG_H

#include <QDialog>
#include <QString>

class QLabel;
class QCheckBox;
class QPushButton;

class AnnouncementDialog : public QDialog {
    Q_OBJECT

public:
    ///
    /// \brief AnnouncementDialog
    /// \param title 弹窗标题
    /// \param content 公告内容（支持HTML）
    /// \param settingsKey 用于记录“不再显示”状态的键名
    /// \param parent
    ///
    explicit AnnouncementDialog(const QString &title, const QString &content,
                                const QString &settingsKey = "ShowGeneralAnnouncement",
                                QWidget *parent = nullptr);
    /* 公告弹窗的持久化逻辑、或settingsKey是怎么用的：
     * 1、键值定义：可以为每个独立公告分配一个唯一的字符串 Key（如 "Welcome_v1"），作为识别凭证。
     * 2、存储方式：布尔值，利用 QSettings 记录在系统注册表中。
     * 3、触发时机：程序启动时按 Key 读取，若无记录默认返回 true 并弹出；用户勾选“不再显示”并确认时，将 Key 设为 false 写入磁盘。
     * 4、版本更迭：发布新版本或新公告时，在代码中改 Key 的后缀（如改为 "Welcome_v2"），或者说是新建一个 Key，就可以弹出想要的弹窗。
     */

private slots:
    void handleAccept();

private:
    QString m_settingsKey;
    QCheckBox *m_dontShowAgainCheck;
};

#endif // ANNOUNCEMENTDIALOG_H