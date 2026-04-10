#ifndef HOMEPAGE_H
#define HOMEPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QList>
#include <QSettings>
#include "modelconfigwidget.h"

class QLabel; // 前向声明

class HomePage : public QWidget {
    Q_OBJECT
public:
    explicit HomePage(const QMap<QString, ModelInfo>& vendorMap, QWidget *parent = nullptr);
    // 获取所有配置（包含主模型和所有现存的副模型）
    QList<ModelConfigWidget::ConfigData> getAllConfigs();
signals:
    void settingsApplied();

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void addFallbackUI();

private:
    const QMap<QString, ModelInfo>& m_vendorMap;
    ModelConfigWidget *mainConfig;      //主模型

    QVBoxLayout *fallbackListLayout;    //副模型容器布局
    struct FallbackItem {
        ModelConfigWidget *configWidget;
        QWidget *container;
        QLabel *titleLabel;
    };
    QList<FallbackItem> m_fallbackItems;

    void saveToSettings();  //UI内容 写入-> 磁盘
    void loadFromSettings();  //磁盘内容 读取-> UI
};

#endif