#ifndef HOMEPAGE_H
#define HOMEPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QList>
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

private slots:
    void addFallbackUI();

private:
    const QMap<QString, ModelInfo>& m_vendorMap;
    ModelConfigWidget *mainConfig;       // 主模型固定不动
    QVBoxLayout *fallbackListLayout;    // 副模型容器布局

    // 内部结构：用于同时管理 Widget、包装容器，以及标题标签（方便动态重命名）
    struct FallbackItem {
        ModelConfigWidget *configWidget;
        QWidget *container;
        QLabel *titleLabel; // 新增：保存标题的引用
    };
    QList<FallbackItem> m_fallbackItems;
};

#endif