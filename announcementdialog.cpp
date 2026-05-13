#include "announcementdialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QSettings>

AnnouncementDialog::AnnouncementDialog(const QString &title, const QString &content,
                                       const QString &settingsKey, QWidget *parent)
    : QDialog(parent), m_settingsKey(settingsKey)
{
    //基础窗口设置
    setWindowTitle("公告");
    setFixedSize(420, 320);
    setStyleSheet("QDialog { background-color: #ffffff; }");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(25, 20, 25, 15);
    mainLayout->setSpacing(15);

    // 1.标题栏
    QLabel *titleLabel = new QLabel(QString("<h3>%1</h3>").arg(title));
    titleLabel->setStyleSheet("color: #0078d4; font-family: 'Microsoft YaHei';");
    mainLayout->addWidget(titleLabel);

    // 2.内容区（支持HTML）
    QLabel *contentLabel = new QLabel(content);
    contentLabel->setWordWrap(true);
    contentLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    contentLabel->setStyleSheet("font-size: 14px; color: #333; line-height: 1.5;");
    mainLayout->addWidget(contentLabel, 1);

    // 3.底部操作区
    QHBoxLayout *bottomLayout = new QHBoxLayout();

    m_dontShowAgainCheck = new QCheckBox("不再显示");
    m_dontShowAgainCheck->setStyleSheet("font-size: 13px; color: #666;");

    QPushButton *okBtn = new QPushButton("确认");
    okBtn->setFixedSize(90, 32);
    okBtn->setCursor(Qt::PointingHandCursor);
    okBtn->setStyleSheet(
        "QPushButton { background-color: #0078d4; color: white; border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background-color: #005a9e; }"
        );

    bottomLayout->addWidget(m_dontShowAgainCheck);
    bottomLayout->addStretch();
    bottomLayout->addWidget(okBtn);

    mainLayout->addLayout(bottomLayout);

    connect(okBtn, &QPushButton::clicked, this, &AnnouncementDialog::handleAccept);
}

void AnnouncementDialog::handleAccept() {
    if (m_dontShowAgainCheck->isChecked()) {
        //使用项目统一的注册表路径
        QSettings settings("Yachi", "PersistentData");
        settings.setValue(m_settingsKey, false);
    }
    accept();
}