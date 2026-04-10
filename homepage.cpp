#include "homepage.h"
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QHBoxLayout>
#include <QMessageBox>

HomePage::HomePage(const QMap<QString, ModelInfo>& vendorMap, QWidget *parent)
    : QWidget(parent), m_vendorMap(vendorMap)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 1. 主模型配置区 (固定)
    mainLayout->addWidget(new QLabel("<h3>核心配置</h3>"));
    QFrame *mainFrame = new QFrame();
    mainFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
    QVBoxLayout *mainFrameLayout = new QVBoxLayout(mainFrame);

    mainConfig = new ModelConfigWidget(m_vendorMap, this);
    mainFrameLayout->addWidget(new QLabel("<b>主模型 (优先调用)</b>"));
    mainFrameLayout->addWidget(mainConfig);
    mainLayout->addWidget(mainFrame);

    mainLayout->addSpacing(10);

    // 2. 副模型配置区 (可动态增删)
    mainLayout->addWidget(new QLabel("<h3>备选模型 (当主模型失效时按顺序切换)</h3>"));

    QScrollArea *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    QWidget *scrollContent = new QWidget();
    fallbackListLayout = new QVBoxLayout(scrollContent);
    fallbackListLayout->setAlignment(Qt::AlignTop);
    scroll->setWidget(scrollContent);
    mainLayout->addWidget(scroll);

    // 3. 操作按钮区
    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *addBtn = new QPushButton("+ 添加备用模型");
    addBtn->setFixedHeight(35);
    addBtn->setStyleSheet("QPushButton { background-color: #f0f0f0; border: 1px dashed #999; } QPushButton:hover { background-color: #e0e0e0; }");

    QPushButton *saveBtn = new QPushButton("保存并应用配置");
    saveBtn->setFixedHeight(35);
    saveBtn->setStyleSheet("background-color: #0078d4; color: white; font-weight: bold; border-radius: 4px;");

    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(saveBtn);
    mainLayout->addLayout(btnLayout);

    // 信号连接
    connect(addBtn, &QPushButton::clicked, this, &HomePage::addFallbackUI);

    //“保存并应用配置”按钮交互信号
    connect(saveBtn, &QPushButton::clicked, this, [this,saveBtn]() {
        saveToSettings();  //数据保存到注册表（磁盘）里

        //当用户点击保存时，触发settingsApplied信号
        emit settingsApplied();

        //弹出一个简单的反馈（可选）
        QMessageBox::information(this, "配置", "配置已成功保存并应用！");
    });

    //加载数据并构建对应UI
    loadFromSettings();
}

void HomePage::addFallbackUI() {
    // 创建一个带边框的卡片容器
    QFrame *itemContainer = new QFrame();
    itemContainer->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    itemContainer->setStyleSheet("QFrame { background-color: #fafafa; border-radius: 5px; margin: 2px; }");

    QVBoxLayout *itemLayout = new QVBoxLayout(itemContainer);

    // 顶部工具栏：显示标题和删除按钮
    QHBoxLayout *headerLayout = new QHBoxLayout();

    // 实例化标题标签，并保存指针
    QLabel *titleLabel = new QLabel(QString("<b>备用模型 #%1</b>").arg(m_fallbackItems.size() + 1));
    headerLayout->addWidget(titleLabel);

    headerLayout->addStretch(); // 弹簧

    QPushButton *delBtn = new QPushButton("× 删除");
    delBtn->setFixedSize(50, 22);
    delBtn->setStyleSheet("QPushButton { color: #d32f2f; border: none; font-weight: bold; } QPushButton:hover { background-color: #ffebee; }");
    headerLayout->addWidget(delBtn);

    itemLayout->addLayout(headerLayout);

    // 核心配置组件
    ModelConfigWidget *configWidget = new ModelConfigWidget(m_vendorMap, itemContainer);
    itemLayout->addWidget(configWidget);

    // 将容器加入布局
    fallbackListLayout->addWidget(itemContainer);

    // 存入追踪列表 (包括新增的 titleLabel)
    FallbackItem item = { configWidget, itemContainer, titleLabel };
    m_fallbackItems.append(item);

    // --- 彻底的删除与重排逻辑 ---
    connect(delBtn, &QPushButton::clicked, this, [this, itemContainer]() {
        // 1. 从追踪列表 m_fallbackItems 中移除该项
        for(int i = 0; i < m_fallbackItems.size(); ++i) {
            if(m_fallbackItems[i].container == itemContainer) {
                m_fallbackItems.removeAt(i);
                break;
            }
        }

        // 2. 安全销毁对象 (Qt 的 deleteLater 会自动将其从布局中剔除)
        itemContainer->deleteLater();

        // 3. 重新对剩下的备用模型进行编号，确保 UI 连续性
        for(int i = 0; i < m_fallbackItems.size(); ++i) {
            m_fallbackItems[i].titleLabel->setText(QString("<b>备用模型 #%1</b>").arg(i + 1));
        }
    });
}

///
/// \brief HomePage::getAllConfigs
/// \brief 获取vendormap.json文件中的配置
/// \return
///
QList<ModelConfigWidget::ConfigData> HomePage::getAllConfigs() {
    QList<ModelConfigWidget::ConfigData> list;
    list.append(mainConfig->getConfig());
    for (const auto& item : m_fallbackItems) {
        list.append(item.configWidget->getConfig());
    }
    return list;
}

///
/// \brief HomePage::saveToSettings
/// \brief UI数据写进注册表（磁盘）
///
void HomePage::saveToSettings()
{
    QSettings settings("Yachi","PersistentData"); //分别填写注册表中的组织名和应用名

    auto configs = getAllConfigs();
    settings.beginWriteArray("Models"); //数组键名可以自定义
    for(int i = 0; i<configs.size(); i++)
    {
        settings.setArrayIndex(i);
        settings.setValue("vendor",configs[i].vendor);
        settings.setValue("model",configs[i].model);
        settings.setValue("apiKey", configs[i].apiKey);
        settings.setValue("baseUrl", configs[i].baseUrl);
    }
    settings.endArray();
}

///
/// \brief HomePage::loadFromSettings
/// \brief 注册表（磁盘）数据读取到UI
///
void HomePage::loadFromSettings()
{
    QSettings settings("Yachi","PersistentData");

    int size = settings.beginReadArray("Models");
    //如果数组成员为空
    if(size<=0)
    {
        settings.endArray();
        return;
    }

    // 1.加载模型的持久化数据
    settings.setArrayIndex(0);
    ModelConfigWidget::ConfigData mainData = {
        settings.value("vendor").toString(),
        settings.value("model").toString(),
        settings.value("apiKey").toString(),
        settings.value("baseUrl").toString()
    };
    //阻塞信号防止在设置时触发不必要的刷新逻辑
    mainConfig->blockSignals(true);
    mainConfig->setConfig(mainData);  //更新配置数据
    mainConfig->blockSignals(false);

    // 2.更新homepage UI
    for (const auto& item : m_fallbackItems) {
        fallbackListLayout->removeWidget(item.container); //先从布局移除
        item.container->deleteLater();                    //再销毁
    }
    m_fallbackItems.clear();

    //重新构建副模型UI
    for (int i = 1; i < size; ++i) {
        addFallbackUI(); //这会向m_fallbackItems添加新项
        settings.setArrayIndex(i);

        ModelConfigWidget::ConfigData subData = {
            settings.value("vendor").toString(),
            settings.value("model").toString(),
            settings.value("apiKey").toString(),
            settings.value("baseUrl").toString()
        };

        m_fallbackItems.last().configWidget->blockSignals(true);
        m_fallbackItems.last().configWidget->setConfig(subData);
        m_fallbackItems.last().configWidget->blockSignals(false);
    }
    settings.endArray();
}

void HomePage::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    loadFromSettings(); //只要切回这个页面，就强制从磁盘同步数据到UI
}