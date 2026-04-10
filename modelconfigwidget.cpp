#include "modelconfigwidget.h"
#include <QTimer>

ModelConfigWidget::ModelConfigWidget(const QMap<QString, ModelInfo>& vendorMap, QWidget *parent)
    : QWidget(parent), m_vendorMap(vendorMap)
{
    QFormLayout *layout = new QFormLayout(this);

    vendorCombo = new QComboBox();
    vendorCombo->addItems(m_vendorMap.keys());

    modelCombo = new QComboBox();
    apiKeyEdit = new QLineEdit();
    apiKeyEdit->setEchoMode(QLineEdit::Password);
    apiKeyEdit->setPlaceholderText("API Key");

    baseUrlEdit = new QLineEdit();

    layout->addRow("厂商:", vendorCombo);
    layout->addRow("模型:", modelCombo);
    layout->addRow("API Key:", apiKeyEdit);
    layout->addRow("Base URL:", baseUrlEdit);

    connect(vendorCombo, &QComboBox::currentTextChanged, this, [this](const QString &vendor)
            {
        //当值真正改变时才更新，减少逻辑运算（性能优化）
        static QString lastVendor;  //记录当前值，防止重复触发
        if (vendor == lastVendor) return;
        lastVendor = vendor;

        //将更新逻辑推入事件队列，给QComboBox留出“收起下拉框”的时间（性能优化）
        QMetaObject::invokeMethod(this, [this, vendor] {
            this->onVendorChanged(vendor);
        }, Qt::QueuedConnection);
    });

    // 初始化首行
    if (!m_vendorMap.isEmpty()) {
        onVendorChanged(vendorCombo->currentText());
    }
}

void ModelConfigWidget::onVendorChanged(const QString &vendor) {
    if (m_vendorMap.contains(vendor)) {
        //操作前阻塞信号，防止modelCombo频繁触发indexChanged
        modelCombo->blockSignals(true);

        modelCombo->clear();
        modelCombo->addItems(m_vendorMap[vendor].models);
        baseUrlEdit->setText(m_vendorMap[vendor].defaultBaseUrl);

        modelCombo->blockSignals(false);
    }
}

ModelConfigWidget::ConfigData ModelConfigWidget::getConfig() {
    return {
        vendorCombo->currentText(),
        modelCombo->currentText(),
        apiKeyEdit->text().trimmed(),
        baseUrlEdit->text().trimmed()
    };
}

void ModelConfigWidget::setConfig(const ConfigData &data)
{
    vendorCombo->setCurrentText(data.vendor);
    // 触发 vendor 改变后，modelCombo 会自动刷新，由于是同步的，可以直接设置
    modelCombo->setCurrentText(data.model);
    apiKeyEdit->setText(data.apiKey);
    baseUrlEdit->setText(data.baseUrl);
}