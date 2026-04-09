#include "homepage.h"
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QHBoxLayout>

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

    // 默认添加一个副模型框
    addFallbackUI();
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

QList<ModelConfigWidget::ConfigData> HomePage::getAllConfigs() {
    QList<ModelConfigWidget::ConfigData> list;
    list.append(mainConfig->getConfig());
    for (const auto& item : m_fallbackItems) {
        list.append(item.configWidget->getConfig());
    }
    return list;
}