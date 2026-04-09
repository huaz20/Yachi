#ifndef MODELCONFIGWIDGET_H
#define MODELCONFIGWIDGET_H

#include <QWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QFormLayout>
#include <QMap>

struct ModelInfo {
    QStringList models;
    QString defaultBaseUrl;
};

class ModelConfigWidget : public QWidget {
    Q_OBJECT
public:
    explicit ModelConfigWidget(const QMap<QString, ModelInfo>& vendorMap, QWidget *parent = nullptr);

    struct ConfigData {
        QString vendor;
        QString model;
        QString apiKey;
        QString baseUrl;
    };
    ConfigData getConfig();
    void setConfig(const ConfigData &data);

private slots:
    void onVendorChanged(const QString &vendor);

private:
    QComboBox *vendorCombo;
    QComboBox *modelCombo;
    QLineEdit *apiKeyEdit;
    QLineEdit *baseUrlEdit;
    const QMap<QString, ModelInfo>& m_vendorMap;
};
#endif