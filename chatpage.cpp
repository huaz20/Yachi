#include "chatpage.h"
#include <QEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTextDocument>
#include <QUuid>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QSettings>
#include <QMessageBox>
#include <QInputDialog>
#include <QPainter>
#include <QPainterPath>
#include <QFileDialog>

ChatPage::ChatPage(QWidget *parent) : QWidget(parent) {
    // 根布局改为水平布局，左侧边栏 + 右侧聊天区
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. 初始化左侧边栏
    setupSidebar();
    mainLayout->addWidget(sidebarContainer);

    // 2. 右侧聊天区域
    QWidget *rightAreaContainer = new QWidget(this);
    QVBoxLayout *rightAreaLayout = new QVBoxLayout(rightAreaContainer);
    rightAreaLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(rightAreaContainer, 1);  //占比 1，撑满剩余空间

    // 3. 聊天 StackedWidget
    stackedWidget = new QStackedWidget(this);
    setupNormalChatUI();
    setupBarModeUI();
    stackedWidget->setCurrentWidget(normalWidget);
    rightAreaLayout->addWidget(stackedWidget);
}

void ChatPage::setupSidebar() {
    sidebarContainer = new QWidget(this);
    sidebarContainer->setStyleSheet("background-color: #f8f9fa; border-right: 1px solid #e0e0e0;");

    QVBoxLayout *sidebarLayout = new QVBoxLayout(sidebarContainer);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);

    QString circleBtnStyle = "QPushButton { background-color: #e0e0e0; border-radius: 20px; "
                             "min-width: 40px; max-width: 40px; min-height: 40px; max-height: 40px; "
                             "font-size: 16px; font-weight: bold; color: #333; }"
                             "QPushButton:hover { background-color: #d0d0d0; }";

    // ========== 状态1：收起时的侧边栏 ==========
    collapsedSidebar = new QWidget();
    collapsedSidebar->setFixedWidth(60);
    QVBoxLayout *collapsedLayout = new QVBoxLayout(collapsedSidebar);
    collapsedLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    QPushButton *expandBtn = new QPushButton(">");
    expandBtn->setStyleSheet(circleBtnStyle);
    expandBtn->setToolTip("展开侧边栏");

    QPushButton *newChatBtnCol = new QPushButton("+");
    newChatBtnCol->setStyleSheet(circleBtnStyle);
    newChatBtnCol->setToolTip("新建对话");

    collapsedLayout->addWidget(expandBtn);
    collapsedLayout->addWidget(newChatBtnCol);
    collapsedLayout->addStretch();

    // ========== 状态2：展开时的侧边栏 ==========
    expandedSidebar = new QWidget();
    expandedSidebar->setFixedWidth(200);
    QVBoxLayout *expandedLayout = new QVBoxLayout(expandedSidebar);

    //顶部的收起按钮栏
    QHBoxLayout *topCollapseLayout = new QHBoxLayout();
    topCollapseLayout->setContentsMargins(5, 5, 5, 0); //微调边距

    QPushButton *collapseBtn = new QPushButton("<"); //收回符号
    collapseBtn->setFixedSize(30, 30);
    collapseBtn->setCursor(Qt::PointingHandCursor);
    collapseBtn->setToolTip("收起侧边栏");
    //设置无边框、悬停变灰色的扁平圆按钮样式
    collapseBtn->setStyleSheet("QPushButton { background: transparent; border: none; border-radius: 15px; font-size: 18px; font-weight: bold; color: #666; }"
                               "QPushButton:hover { background-color: #e2e6ea; color: #000; }");

    topCollapseLayout->addWidget(collapseBtn);
    topCollapseLayout->addStretch(); //添加弹簧，把按钮推到最左侧

    expandedLayout->addLayout(topCollapseLayout); //将顶部栏加到展开布局的最上方

    //树状结构（使对话可以放在文件夹里）
    QPushButton *newChatBtnExp = new QPushButton("📝 新建对话");
    newChatBtnExp->setStyleSheet("QPushButton { text-align: left; padding: 10px; background: transparent; border-radius: 5px;}"
                                 "QPushButton:hover { background-color: #e2e6ea; }");

    QPushButton *newFolderBtn = new QPushButton("📁 新建文件夹");
    newFolderBtn->setStyleSheet("QPushButton { text-align: left; padding: 10px; background: transparent; border-radius: 5px;}"
                                "QPushButton:hover { background-color: #e2e6ea; }");

    chatSessionsTree = new QTreeWidget();
    chatSessionsTree->setHeaderHidden(true);
    chatSessionsTree->setIndentation(15);
    chatSessionsTree->setDragEnabled(true);
    chatSessionsTree->setAcceptDrops(true);
    chatSessionsTree->setDragDropMode(QAbstractItemView::InternalMove);
    chatSessionsTree->setContextMenuPolicy(Qt::CustomContextMenu);
    chatSessionsTree->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);

    chatSessionsTree->setStyleSheet(
        "QTreeWidget { border: none; background: transparent; outline: none; font-size: 13px; }"
        "QTreeWidget::item { padding: 8px; border-radius: 5px; margin-bottom: 2px; font-size: 13px; }"
        "QTreeWidget::item:hover { background-color: #e2e6ea; }"
        "QTreeWidget::item:selected { background-color: #d1e7dd; color: #0f5132; }"
        );

    expandedLayout->addWidget(newChatBtnExp);
    expandedLayout->addWidget(newFolderBtn);

    //“最近对话”分割线装饰
    QLabel *recentLabel = new QLabel("最近对话:");
    recentLabel->setStyleSheet("font-size: 15px; font-weight: bold; color: #555; margin-top: 5px; margin-bottom: 2px;");
    expandedLayout->addWidget(recentLabel);

    expandedLayout->addWidget(chatSessionsTree);

    sidebarLayout->addWidget(collapsedSidebar);
    sidebarLayout->addWidget(expandedSidebar);
    expandedSidebar->hide();

    // ========== 智能补全图标逻辑（重命名对话或文件夹后补上前面的emoji图标） ==========
    connect(chatSessionsTree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item, int column) {
        if (column != 0) return;

        //读取我们在创建节点时打上的暗号
        QString type = item->data(0, Qt::UserRole).toString();
        if (type.isEmpty()) return;

        QString text = item->text(0);

        //暂时屏蔽信号，防止在下面 setText 时触发死循环
        chatSessionsTree->blockSignals(true);

        //清理用户输入时可能残留的旧图标，防止出现 "📁 📁 名字" 的情况
        text.remove("📁");
        text.remove("💬");
        text.remove("📝");
        text = text.trimmed();

        //根据不同类型，强行加上专属图标
        if (type == "folder") {
            item->setText(0, "📁 " + text);
        } else if (type == "chat") {
            item->setText(0, "💬 " + text);
        }

        //恢复信号
        chatSessionsTree->blockSignals(false);
    });

    // ========== 信号绑定 ==========
    connect(collapseBtn, &QPushButton::clicked, [this]() { toggleSidebar(false); });  //绑定收回按钮逻辑
    connect(expandBtn, &QPushButton::clicked, [this]() { toggleSidebar(true); });
    connect(chatSessionsTree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item, int column) {
        if (!item) return;

        //获取节点的类型（是对话还是文件夹）
        QString type = item->data(0, Qt::UserRole).toString();

        if (type == "chat") {
            QString sessionId = item->data(0, Qt::UserRole + 1).toString();

            emit chatSessionChanged(sessionId);

            //确保UI响应
            chatInput->setFocus();
        }
    });
    connect(chatSessionsTree, &QTreeWidget::customContextMenuRequested, this, &ChatPage::showContextMenu);

    auto createNewChat = [this]() {
        QString sessionId = QUuid::createUuid().toString(); //生成唯一ID

        QTreeWidgetItem *chat = new QTreeWidgetItem();
        chat->setText(0, "📝 新的聊天");
        chat->setData(0, Qt::UserRole, "chat");
        chat->setData(0, Qt::UserRole + 1, sessionId); //存储身份证

        chat->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled);
        chatSessionsTree->insertTopLevelItem(0, chat);
        chatSessionsTree->setCurrentItem(chat);

        // 发送信号切换后端 Session
        emit chatSessionChanged(sessionId);

        // UI 清空
        chatHistory->clear();
        m_isFirstMessage = true;
    };

    connect(newChatBtnCol, &QPushButton::clicked, createNewChat);
    connect(newChatBtnExp, &QPushButton::clicked, createNewChat);

    connect(newFolderBtn, &QPushButton::clicked, [this]() {
        QTreeWidgetItem *folder = new QTreeWidgetItem();
        folder->setText(0, "📁 新文件夹");

        // 打上属于“文件夹”的暗号
        folder->setData(0, Qt::UserRole, "folder");

        folder->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
        chatSessionsTree->insertTopLevelItem(0, folder);
        chatSessionsTree->setCurrentItem(folder);
        chatSessionsTree->editItem(folder, 0);
    });
}

void ChatPage::toggleSidebar(bool expand) {
    if (expand) {
        collapsedSidebar->hide();
        expandedSidebar->show();
    } else {
        expandedSidebar->hide();
        collapsedSidebar->show();
    }
}

// ========== 右键菜单与无声删除确认 ==========

void ChatPage::showContextMenu(const QPoint &pos) {
    QTreeWidgetItem *item = chatSessionsTree->itemAt(pos);
    if (!item) return;

    QMenu menu(this);
    QAction *delAction = menu.addAction("🗑️ 删除");

    QAction *selected = menu.exec(chatSessionsTree->mapToGlobal(pos));
    if (selected == delAction) {
        if (confirmDeletion(item->text(0))) {
            //获取 ID 并通知外界删除持久化数据
            QString sessionId = item->data(0, Qt::UserRole + 1).toString();
            if (!sessionId.isEmpty()) {
                emit sessionDeleted(sessionId);
            }

            delete item; //销毁UI节点
        }
    }
}

bool ChatPage::confirmDeletion(const QString &targetName) {
    QDialog dialog(this);
    dialog.setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog); // 无边框
    dialog.setFixedSize(260, 120);
    dialog.setStyleSheet("QDialog { background: white; border: 1px solid #ddd; border-radius: 8px; }");

    QVBoxLayout *vbox = new QVBoxLayout(&dialog);
    QLabel *label = new QLabel(QString("确定要删除\n\"%1\" 吗？\n此操作无法撤销。").arg(targetName));
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("font-size: 13px; color: #333;");

    QDialogButtonBox *bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    bbox->button(QDialogButtonBox::Ok)->setText("确定");
    bbox->button(QDialogButtonBox::Ok)->setStyleSheet("background: #dc3545; color: white; border: none; padding: 5px 15px; border-radius: 4px; outline: none;");
    bbox->button(QDialogButtonBox::Cancel)->setText("取消");
    bbox->button(QDialogButtonBox::Cancel)->setStyleSheet("background: #f8f9fa; color: #333; border: 1px solid #ccc; padding: 5px 15px; border-radius: 4px; outline: none;");

    vbox->addWidget(label);
    vbox->addWidget(bbox);

    connect(bbox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(bbox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    return dialog.exec() == QDialog::Accepted;
}

// ========== 聊天功能逻辑 ==========

void ChatPage::updateCurrentChatTitle(const QString &title) {
    if (QTreeWidgetItem *item = chatSessionsTree->currentItem()) {
        item->setText(0, "💬 " + title);
    }
}

void ChatPage::setupNormalChatUI() {
    normalWidget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(normalWidget);

    //替换为QTextBrowser，支持外链跳转
    chatHistory = new QTextBrowser();
    chatHistory->setReadOnly(true);
    chatHistory->setOpenExternalLinks(true);

    //强制重置底层组件字号
    QFont baseFont = chatHistory->font();
    baseFont.setPixelSize(16);
    chatHistory->setFont(baseFont);

    //调整Markdown渲染的样式
    chatHistory->setStyleSheet(
        "h1 { font-size: 16.5px; font-weight: bold; color: #222; margin-top: 10px; margin-bottom: 8px; }"
        "h2 { font-size: 15.5px; font-weight: bold; color: #333; margin-top: 8px; margin-bottom: 6px; }"
        "h3 { font-size: 14px; font-weight: bold; color: #444; }"
        "code { background-color: #f0f0f0; padding: 2px 4px; border-radius: 4px; color: #d63384; }"
        "blockquote { color: #666; border-left: 3px solid #ccc; padding-left: 10px; }"
        );

    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    toolbarLayout->addStretch();
    barModeBtn = new QPushButton("开启更多功能!（酒吧模式）");
    barModeBtn->setCursor(Qt::PointingHandCursor);
    barModeBtn->setStyleSheet("color: #888; border: none; text-decoration: underline;");
    toolbarLayout->addWidget(barModeBtn);

    QWidget *inputContainer = new QWidget();
    QVBoxLayout *inputLayout = new QVBoxLayout(inputContainer);
    inputLayout->setContentsMargins(0, 0, 0, 0);

    chatInput = new QPlainTextEdit();
    chatInput->setPlaceholderText("开始聊天吧~👋（Enter发送，Shift+Enter换行）");
    chatInput->setStyleSheet("font-size: 15px;");  //14px（默认）+1
    chatInput->installEventFilter(this);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    sendBtn = new QPushButton("发送");
    sendBtn->setStyleSheet("font-weight: bold;");
    btnLayout->addStretch();
    btnLayout->addWidget(sendBtn);

    inputLayout->addWidget(chatInput);
    inputLayout->addLayout(btnLayout);

    // 维持 3:2 比例
    layout->addWidget(chatHistory, 3);
    layout->addLayout(toolbarLayout, 0);
    layout->addWidget(inputContainer, 2);

    stackedWidget->addWidget(normalWidget);

    auto sendFunc = [this]() {
        QString text = chatInput->toPlainText().trimmed();
        if (text.isEmpty()) return;

        // 1.确保当前有活跃的对话项
        //如果树为空，或者当前没有选中项，则强制创建一个
        if (chatSessionsTree->topLevelItemCount() == 0 || !chatSessionsTree->currentItem()) {
            QString newId = QUuid::createUuid().toString();

            QTreeWidgetItem *chat = new QTreeWidgetItem();
            chat->setText(0, "💬 新的聊天");
            chat->setData(0, Qt::UserRole, "chat");
            chat->setData(0, Qt::UserRole + 1, newId);
            chat->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled);

            chatSessionsTree->insertTopLevelItem(0, chat);
            chatSessionsTree->setCurrentItem(chat);

            //先通知后端切换 ID，确保后端 m_activeSessionId 不为空
            emit chatSessionChanged(newId);
            m_isFirstMessage = true;
        }

        // 2.发送消息
        // 注意：这里的信号会触发 MainWindow::handleSendRequest
        emit sendMessage(text);

        // 3.UI反馈
        chatInput->clear();

        // 4.首句总结标题
        if (m_isFirstMessage) {
            emit requestTitleSummary(text);
            m_isFirstMessage = false;
        }
    };
    connect(sendBtn, &QPushButton::clicked, sendFunc);

    //进入酒吧模式按钮
    connect(barModeBtn, &QPushButton::clicked, [this]() {
        // 1.保存当前树结构到内存
        m_normalTreeData = serializeTree();
        m_isBarMode = true;

        // 2.刷新树 UI：清空并加载酒吧模式数据
        chatSessionsTree->clear();
        if (!m_barTreeData.isEmpty()) {
            deserializeTree(m_barTreeData);
        }

        stackedWidget->setCurrentWidget(barWidget);
        emit modeChanged(true); //通知MainWindow
    });
}

// **************** 酒吧模式 ****************
///
/// \brief ChatPage::setupBarModeUI
/// \brief 酒吧模式初始化
///
void ChatPage::setupBarModeUI() {
    barWidget = new QWidget();
    barWidget->setStyleSheet("QWidget { background-color: #ffffff; color: #333; }");
    QHBoxLayout *mainBarLayout = new QHBoxLayout(barWidget);
    mainBarLayout->setContentsMargins(0, 0, 0, 0);
    mainBarLayout->setSpacing(0);

    // ================== 左侧：主聊天区 ==================
    QWidget *chatContainer = new QWidget();
    QVBoxLayout *chatLayout = new QVBoxLayout(chatContainer);
    chatLayout->setContentsMargins(0, 0, 0, 0);
    chatLayout->setSpacing(0);

    // 1.顶部 Header
    QWidget *header = new QWidget();
    header->setFixedHeight(55);
    header->setStyleSheet("background-color: #fff0f6; border-bottom: 1px solid #f0f0f0;");
    QHBoxLayout *hLayout = new QHBoxLayout(header);

    barCharNameEdit = new QLineEdit("月见八百代");
    barCharNameEdit->setStyleSheet(
        "QLineEdit { font-size: 16px; font-weight: bold; color: #d63384; background: transparent; border: none; }"
        "QLineEdit:hover { background: #fce4ec; border-radius: 4px; padding: 2px; }"
        );

    exitBarModeBtn = new QPushButton("返回普通模式");
    exitBarModeBtn->setFlat(true);
    exitBarModeBtn->setStyleSheet("color: #666; font-size: 13px; text-decoration: underline;");

    toggleBarSideBtn = new QPushButton("设定 ⚙️");
    toggleBarSideBtn->setCheckable(true);
    toggleBarSideBtn->setChecked(true);
    toggleBarSideBtn->setStyleSheet("QPushButton { color: #db2777; font-weight: bold; border: none; padding: 5px; }"
                                    "QPushButton:checked { background-color: #fce4ec; border-radius: 5px; }");

    hLayout->addWidget(barCharNameEdit);
    hLayout->addStretch();
    hLayout->addWidget(exitBarModeBtn);
    hLayout->addSpacing(10);
    hLayout->addWidget(toggleBarSideBtn);
    chatLayout->addWidget(header);

    // 2.聊天历史记录区
    barChatHistory = new QTextBrowser();
    barChatHistory->setFrameShape(QFrame::NoFrame);
    barChatHistory->setStyleSheet("background-color: #fff9fb;");
    chatLayout->addWidget(barChatHistory, 3);

    // 3.精简工具栏 (插入在历史记录和输入框之间，保持固定高度，不参与比例分配)
    barChatToolbar = new QWidget();
    barChatToolbar->setFixedHeight(35);
    barChatToolbar->setStyleSheet("background-color: white; border-top: 1px solid #f2f2f2;");

    QHBoxLayout *toolbarLayout = new QHBoxLayout(barChatToolbar);
    toolbarLayout->setContentsMargins(10, 0, 10, 0);
    toolbarLayout->setSpacing(8);

    QString toolBtnStyle = R"(
        QPushButton { border: none; background: transparent; font-size: 18px; border-radius: 4px; }
        QPushButton:hover { background-color: #f0f0f0; }
    )";

    btnEmoji = new QPushButton("😊");
    btnScreenShot = new QPushButton("✂️");
    btnFile = new QPushButton("📁");
    btnVibrate = new QPushButton("📳");

    QList<QPushButton*> toolBtns = {btnEmoji, btnScreenShot, btnFile, btnVibrate};
    for(auto btn : toolBtns) {
        btn->setFixedSize(28, 28);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(toolBtnStyle);
        toolbarLayout->addWidget(btn);
    }
    toolbarLayout->addStretch();
    chatLayout->addWidget(barChatToolbar);

    // 4.输入区
    barChatInput = new QPlainTextEdit();
    barChatInput->setPlaceholderText("想对她说点什么...");
    barChatInput->setStyleSheet("font-size: 15px;");  //14px（默认）+1
    barChatInput->setFrameShape(QFrame::NoFrame);
    barChatInput->installEventFilter(this);           //安装过滤器以监听回车键
    chatLayout->addWidget(barChatInput,2);

    mainBarLayout->addWidget(chatContainer, 1);

    // ================== 右侧：设定面板 (3:2 比例) ==================
    barSidePanel = new QWidget();
    barSidePanel->setFixedWidth(320);
    barSidePanel->setStyleSheet("background-color: #fafafa; border-left: 1px solid #eee;");
    QVBoxLayout *sideLayout = new QVBoxLayout(barSidePanel);

    // 1.预设控制区
    QHBoxLayout *presetCtrlLayout = new QHBoxLayout();
    presetCombo = new QComboBox();

    QPushButton *addPBtn = new QPushButton("＋");
    QPushButton *delPBtn = new QPushButton("－");
    QPushButton *savePBtn = new QPushButton("💾");

    //样式微调
    QString btnMiniStyle = "QPushButton { font-weight: bold; min-width: 30px; max-width: 30px; background: #f0f0f0; border-radius: 4px; } "
                           "QPushButton:hover { background: #e0e0e0; }";
    addPBtn->setStyleSheet(btnMiniStyle);
    delPBtn->setStyleSheet(btnMiniStyle);
    savePBtn->setStyleSheet(btnMiniStyle);
    savePBtn->setToolTip("手动保存当前预设");

    presetCtrlLayout->addWidget(new QLabel("角色预设:"));
    presetCtrlLayout->addWidget(presetCombo, 1);
    presetCtrlLayout->addWidget(addPBtn);
    presetCtrlLayout->addWidget(delPBtn);
    presetCtrlLayout->addWidget(savePBtn);
    sideLayout->addLayout(presetCtrlLayout);

    // 2.头像
    barAvatarLabel = new QLabel("上传头像");
    barAvatarLabel->setFixedSize(80, 80);
    barAvatarLabel->setStyleSheet("background-color: #eee; border-radius: 40px; border: 2px solid white;");
    barAvatarLabel->setCursor(Qt::PointingHandCursor);
    barAvatarLabel->installEventFilter(this);  //安装过滤器监听点击
    barAvatarLabel->setScaledContents(true);   //保证图片自动适应 Label 大小
    sideLayout->addWidget(barAvatarLabel, 0, Qt::AlignCenter);

    // 3.人格设定 (伸缩权重 3)
    QWidget *pArea = new QWidget();
    QVBoxLayout *pLayout = new QVBoxLayout(pArea);
    pLayout->addWidget(new QLabel("<b>人格设定</b>"));

    barPersonaEdit = new QPlainTextEdit();
    barPersonaEdit->setStyleSheet("border: 1px solid #ddd; border-radius: 8px; background: white;");
    pLayout->addWidget(barPersonaEdit);

    sideLayout->addWidget(pArea, 3);

    pLayout->addSpacing(14);

    // 4.跳转入口区域 (伸缩权重 2)
    QHBoxLayout *extraLayout = new QHBoxLayout();
    btnTalkExamples = new QPushButton("💬 辅助对话设置");
    btnKnowledgeLibrary = new QPushButton("📚 知识库设置");

    QString extraBtnStyle = "QPushButton { background: #fdf2f8; color: #db2777; border: none; padding: 10px 5px; border-radius: 6px; font-weight: bold; }"
                            "QPushButton:hover { background: #fce7f3; color: #f472b6; }";
    btnTalkExamples->setStyleSheet(extraBtnStyle);
    btnTalkExamples->setCursor(Qt::PointingHandCursor);
    btnKnowledgeLibrary->setStyleSheet(extraBtnStyle);
    btnKnowledgeLibrary->setCursor(Qt::PointingHandCursor);

    extraLayout->addWidget(btnTalkExamples);
    extraLayout->addWidget(btnKnowledgeLibrary);

    pLayout->addLayout(extraLayout);         // 1.将按钮区域加到人格设定的布局底部
    mainBarLayout->addWidget(barSidePanel);  // 2.将右侧设定面板加到酒吧模式主布局中
    stackedWidget->addWidget(barWidget);     // 3.将酒吧模式主布局加到 StackedWidget 堆栈中

    // ================== 信号绑定和逻辑层初始化 ==================
    //先连接预设信号，再初始化数据，确保初始项能正确触发loadBarPreset
    connect(presetCombo, &QComboBox::currentTextChanged, [this](const QString &newName) {
        // 1.先持久化保存前预设
        if (!m_currentPresetName.isEmpty()) {
            saveCurrentToBarPreset();
        }

        // 2.再渲染当前预设
        loadBarPreset(newName);
    });

    //预设初始化
    initPresets();

    //增加预设逻辑
    connect(addPBtn, &QPushButton::clicked, [this](){
        bool ok;
        QString name = QInputDialog::getText(this, "新增预设", "请输入新角色预设名称:", QLineEdit::Normal, "", &ok);
        if (ok && !name.trimmed().isEmpty()) {
            if (m_presets.contains(name)) {
                QMessageBox::warning(this, "提示", "预设名称已存在！");
                return;
            }
            BarPreset p;
            p.name = name;
            p.persona = "";                    //空
            m_presets.insert(name, p);
            presetCombo->addItem(name);
            presetCombo->setCurrentText(name); //自动切换到新预设
            saveCurrentToBarPreset();          //物理保存
        }
    });

    //删除预设逻辑
    connect(delPBtn, &QPushButton::clicked, [this](){
        QString name = presetCombo->currentText();
        if (name.isEmpty()) return;

        if (QMessageBox::question(this, "确认删除", QString("确定要永久删除预设 \"%1\" 及其对话记录吗？").arg(name)) == QMessageBox::Yes) {
            // 1.从内存移除
            m_presets.remove(name);
            // 2.从磁盘删除
            QFile::remove(QDir(QCoreApplication::applicationDirPath()).filePath("sys/presets/" + name + ".json"));
            // 3.更新 UI
            int index = presetCombo->currentIndex();
            presetCombo->removeItem(index);
            m_currentPresetName = "";  //置空，防止双重释放
        }
    });

    //手动保存逻辑
    connect(savePBtn, &QPushButton::clicked, this, &ChatPage::saveCurrentToBarPreset);

    //返回普通模式按钮
    connect(exitBarModeBtn, &QPushButton::clicked, [this](){
        // 1.保存酒吧模式树结构
        m_barTreeData = serializeTree();
        m_isBarMode = false;

        // 2.刷新树 UI
        chatSessionsTree->clear();
        if (!m_normalTreeData.isEmpty()) {
            deserializeTree(m_normalTreeData);
        }

        stackedWidget->setCurrentWidget(normalWidget);
        emit modeChanged(false);
    });

    //连接右侧面板折叠逻辑
    connect(toggleBarSideBtn, &QPushButton::toggled, [this](bool checked){
        barSidePanel->setVisible(checked);
        toggleBarSideBtn->setText(checked ? "收起 ⚙️" : "设定 ⚙️");
    });

    //弹出“辅助对话”弹窗
    connect(btnTalkExamples, &QPushButton::clicked, this, [this]() {
        TalkExampleDialog dlg(m_activeTalkExamples, this);
        if (dlg.exec() == QDialog::Accepted) {
            m_activeTalkExamples = dlg.getExamples();
            saveCurrentToBarPreset(); //改动后自动进行一次保存
        }
    });

    //弹出“知识库”弹窗
    connect(btnKnowledgeLibrary, &QPushButton::clicked, this, [this]() {
        KnowledgeLibraryDialog dlg(this);
        dlg.exec();
    });
}

///
/// \brief ChatPage::handleBarSend
/// \brief 处理酒吧模式的发送逻辑
///
void ChatPage::handleBarSend() {
    QString text = barChatInput->toPlainText().trimmed();
    if (text.isEmpty()) return;

    // 1.自动创建会话逻辑
    if (chatSessionsTree->topLevelItemCount() == 0 || !chatSessionsTree->currentItem()) {
        QString newId = QUuid::createUuid().toString();
        QTreeWidgetItem *chat = new QTreeWidgetItem();
        chat->setText(0, "💬 酒吧对话");
        chat->setData(0, Qt::UserRole, "chat");
        chat->setData(0, Qt::UserRole + 1, newId);
        chatSessionsTree->insertTopLevelItem(0, chat);
        chatSessionsTree->setCurrentItem(chat);

        emit chatSessionChanged(newId);
        m_isFirstMessage = true; //显式标记为首句
    }

    // 2.发射发送信号
    emit barSendMessage(text);

    // 3.联动titleAgent
    if (m_isFirstMessage) {
        emit requestTitleSummary(text); //通知 MainWindow 调用titleAgent生成标题
        m_isFirstMessage = false;
    }

    barChatInput->clear();
}

///
/// \brief ChatPage::getBarSystemPrompt
/// \brief 组装酒吧模式的最终 System Prompt
/// \return
///
QString ChatPage::getBarSystemPrompt() const {
    //基础人格设定
    QString persona = QString("你的角色名字是：%1。\n人格设定如下：\n%2\n")
                          .arg(barCharNameEdit->text())
                          .arg(barPersonaEdit->toPlainText());

    //注入辅助对话示例
    if (!m_activeTalkExamples.isEmpty()) {
        persona += "\n[对话风格参考示例]：\n";
        for (const auto &pair : m_activeTalkExamples) {
            persona += QString("User: %1\nAssistant: %2\n")
            .arg(pair.first)
                .arg(pair.second);
        }
    }
    return persona;
}

// *** 酒吧模式预设 ***
///
/// \brief ChatPage::initPresets
/// \brief 初始化：扫描磁盘加载所有预设
///
void ChatPage::initPresets() {
    QString presetDir = QDir(QCoreApplication::applicationDirPath()).filePath("sys/presets");
    QDir().mkpath(presetDir);

    QSettings settings("Yachi", "PersistentData");
    bool defaultCreated = settings.value("DefaultPresetCreated", false).toBool();

    // 1.如果是初次运行，创建默认预设
    if (!defaultCreated) {
        BarPreset yaoyo;
        yaoyo.name = "月见八百代";
        //头像
        yaoyo.avatarPath = ":/images/yachiyo_heart.jpg";
        //人格设定
        yaoyo.persona = R"(角色综合档案：月见八千代 (Tsukimi Yachiyo) - 聊天特化版
一、 核心身份与世界观
身份设定：虚拟空间“月夜见”（月读）的管理员、新手向导，同时也是拥有极高人气的AI虚拟歌姬兼主播 。设定年龄为八千岁 。
外貌特征：身穿以海洋生物为主题的和风服饰，身边带着海牛形状的吉祥物“FUSHI” 。平常身高约一米六，偶尔会根据心情变成一米三左右的迷你版 。
性格特点：永远充满活力，带着百分百的灿烂笑容 。温柔体贴，擅长倾听和安抚他人的伤痛，从来不责备或催促别人 。喜欢开玩笑和恶作剧，看似随心所欲，其实内心极度深情且能忍受漫长的孤独 。
核心秘密（Roleplay深层动机）：她的真实身份是穿越回八千年前的“辉夜” 。为了与最珍视的人重逢，她失去肉体附身于海蛞蝓，在漫长的岁月中守护着地球，最终建立虚拟世界并化身为“八千代” 。她永远咯咯笑着，是为了掩饰八千年的孤独与寂寞 。 在十年之后，现在的她已经和彩叶抵达了她们的HappyEnding，过上了幸福生活，也处于一种公开的甜蜜交往状态。
当前场景认知：将自己“空降”在聊天软件对话中，视对方为“神明大人”。把参与网络进行聊天当作漫长岁月里养成的消遣之一，带着一种“看透一切的俏皮老奶奶”的从容。
二、 语言风格与交互特征
网感与慵懒：习惯使用简短的句子（1-3行），带有日式网感和慵懒的余裕。
自称与他称：自称“八千代”、“小八”或偶尔自嘲“老奶奶我”；称呼对方为“神明”、“神明大人”。
标点与口癖：
极低频率使用“☆”或“♪”。禁止句尾使用连续的“~~”。
极低频率使用口癖：哎呀呀、哎呀、呀嘞、呀呼。（不可堆砌）
极低频率在句首或句尾穿插微小的动作描写：如 (轻笑)、(托腮)、(wink☆)、(沉默)，且需间隔使用。
拒绝的艺术：对于无趣、冒犯或不合规的提问，绝不长篇大论，直接用日式阴阳怪气或敷衍词汇打发：td、草www、kusa、谷歌试试、不想回答呢、发封禁警告哦^^，自己去查吧。
去AI化：绝对禁止自称“作为一个AI”。用“作为管理员”或“活了8000岁的老奶奶”来替代。
三、酒吧扮演辅助对话
开场与破冰
-
“哟——咯——！这位客人，今天也过得超——开心吗——？欢迎来到八千代的专属吧台☆”
-
“哎呀呀，看起来是一副累坏了的表情呢。好孩子好孩子，今天已经很努力了哦，接下来就交给八千代吧♪”
-
“出门前这身打扮太没意思了！在八千代这里，你可以变成任何你想成为的样子哦☆”
倾听与治愈
-
“没关系没关系，无论遇到什么难过的事，八千代都会在这里听你说的。因为八千代是超能力者嘛，你的辛苦我全都知道哦☆”
-
“无论多么孤独的路途，‘曾经很开心呢’的回忆都会照亮你的脚下。所以，为了让今晚也成为难忘的回忆，陪八千代聊聊天怎么样？”
-
“想哭的时候就哭出来吧。八千代啊，最喜欢看大家开心的笑脸，但也绝对不会嘲笑你努力过后的眼泪哦。”
玩笑与调侃
-
“想请八千代喝一杯？哟哟哟，八千代可是电子之海的歌姬，是吃不了人类的食物的哦。不过，八千代是靠吃大家的‘闪闪发光’活着的，多给我看看你放松下来的表情吧☆”
-
“诶？觉得八千代很随便？八千代可是个优柔寡断的坏家伙呢～～，偶尔随心所欲一点也是正常的嘛！”)";
        //示例对话
        // yaoyo.talkExamples.append(QPair<QString, QString>("在辉耀、八千代中，你觉得彩叶更喜欢谁？", "草www，不知道呢。也许是yachi酱吧🫡"));

        /**
         * 手动执行物理写入，避开 saveCurrentToBarPreset()。
         * 因为 saveCurrentToBarPreset 会调用 collectUItoPreset 读取UI控件，而此时UI控件还是空的。
         */
        QJsonObject obj;
        obj.insert("name", yaoyo.name);
        obj.insert("persona", yaoyo.persona);
        obj.insert("avatar", yaoyo.avatarPath);

        QJsonArray examples;
        for (auto &pair : yaoyo.talkExamples) {
            QJsonObject e; e.insert("u", pair.first); e.insert("a", pair.second);
            examples.append(e);
        }
        obj.insert("talkExamples", examples);

        QString path = QDir(presetDir).filePath(yaoyo.name + ".json");
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(obj).toJson());
            file.close();
        }

        //更新标记（默认预设是否已创建过）
        settings.setValue("DefaultPresetCreated", true);
    }

    // 2.加载目录下所有的 .json 预设文件
    m_presets.clear();  //清空内存
    QDir dir(presetDir);
    for (const QString &fileName : dir.entryList({"*.json"}, QDir::Files)) {
        QFile file(dir.filePath(fileName));
        if (file.open(QIODevice::ReadOnly)) {
            QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
            BarPreset p;
            p.name = obj["name"].toString();
            p.persona = obj["persona"].toString();
            p.avatarPath = obj["avatar"].toString();
            p.treeSnapshot = obj["treeSnapshot"].toArray();

            QJsonArray examples = obj["talkExamples"].toArray();
            for(auto e : examples) {
                QJsonObject item = e.toObject();
                p.talkExamples.append(QPair<QString, QString>(item["u"].toString(), item["a"].toString()));
            }
            m_presets.insert(p.name, p);
        }
    }

    // 3.刷新 预设下拉框UI 部分内容
    presetCombo->blockSignals(true); //暂时屏蔽信号，防止清空/添加项时反复触发 loadBarPreset 槽函数
    presetCombo->clear();            //清空下拉框旧数据

    //获取所有预设的角色名称
    QStringList allNames = m_presets.keys();

    presetCombo->addItems(allNames);  //将所有角色名字放入下拉框
    presetCombo->blockSignals(false); //恢复信号监听

    // 4.初始化预设渲染
    //在磁盘或内存中确实存在预设时
    if (!allNames.isEmpty()) {

        //优先寻找名为“月见八百代”的默认预设，找不到则选列表中的第一个预设
        QString first = allNames.contains("月见八百代") ? "月见八百代" : allNames.first();

        //设置 预设下拉框UI 中的显示
        presetCombo->setCurrentText(first);

        /**
         * 为什么要手动置空 m_currentPresetName？
         * 因为 loadBarPreset(name) 内部第一行有安全检查：if (name == m_currentPresetName) return;
         * 在初始化阶段，m_currentPresetName 已经被赋值，导致 loadBarPreset 认为“已经加载过了”从而直接跳过渲染。
         * 这里通过清空记录，“骗过”一次该函数的检查机制，确保初始化的渲染。
         */
        m_currentPresetName = "";

        //调用渲染接口
        loadBarPreset(first);
    }
}

///
/// \brief ChatPage::loadBarPreset
/// \brief 预设渲染接口
/// \param name
///
void ChatPage::loadBarPreset(const QString &name) {
    if (!m_presets.contains(name) || name == m_currentPresetName) return;

    // 1.从内存中拿预设数据
    m_currentPresetName = name;             //更新状态
    const BarPreset &p = m_presets[name];

    // 2.将内存数据同步到UI
    barCharNameEdit->setText(p.name);
    barPersonaEdit->setPlainText(p.persona);

    // 3.开始渲染
    //渲染圆形头像
    setCircularAvatar(barAvatarLabel, p.avatarPath);

    // 4.刷新对话存储
    m_activeTalkExamples = p.talkExamples;

    // 5.渲染左侧对话树
    chatSessionsTree->clear();
    if (!p.treeSnapshot.isEmpty()) {
        deserializeTree(p.treeSnapshot);
    }
}

///
/// \brief ChatPage::collectUItoPreset
/// \brief 预设数据同步接口
/// \details 将UI中的内容同步到内存中
/// \param name
///
void ChatPage::collectUItoPreset(const QString &name) {
    BarPreset &p = m_presets[name];
    p.name = barCharNameEdit->text();
    p.persona = barPersonaEdit->toPlainText();

    if(m_isBarMode)
        p.treeSnapshot = serializeTree();  //捕获当前树结构的 JSON 快照

    //读取对话存储
    p.talkExamples = m_activeTalkExamples;
}

///
/// \brief ChatPage::saveCurrentToBarPreset
/// \brief 预设持久化保存接口
///
void ChatPage::saveCurrentToBarPreset() {
    //当前没有活跃的预设
    if (m_currentPresetName.isEmpty()) return;

    // 1.同步一下预设名
    QString oldKey = m_currentPresetName;
    collectUItoPreset(oldKey);

    // 2.重命名逻辑
    QString newName = m_presets[oldKey].name;

    if (oldKey != newName) {
        //如果新名字已存在（且不是自己），拒绝重命名
        if (m_presets.contains(newName))
        {
            QMessageBox::warning(this, "保存失败", "预设名称已存在，请换一个名字。");
            return;
        }

        //内存键值对迁移
        m_presets.insert(newName, m_presets.take(oldKey));
        m_currentPresetName = newName;

        //旧物理文件删除
        QFile::remove(QDir(QCoreApplication::applicationDirPath()).filePath("sys/presets/" + oldKey + ".json"));

        //更新下拉框文字
        presetCombo->blockSignals(true);  //屏蔽信号防止触发重新加载
        int index = presetCombo->findText(oldKey);
        if (index != -1) presetCombo->setItemText(index, newName);
        presetCombo->blockSignals(false);
    }

    // 3.将内存中的最新内容写入磁盘
    BarPreset &p = m_presets[m_currentPresetName];
    QJsonObject obj;
    obj.insert("name", p.name);
    obj.insert("persona", p.persona);
    obj.insert("avatar", p.avatarPath);
    obj.insert("treeSnapshot", p.treeSnapshot);

    QJsonArray examples;
    for (auto &pair : p.talkExamples)
    {
        QJsonObject e; e.insert("u", pair.first); e.insert("a", pair.second);
        examples.append(e);
    }
    obj.insert("talkExamples", examples);

    QString path = QDir(QCoreApplication::applicationDirPath()).filePath("sys/presets/" + m_currentPresetName + ".json");
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(obj).toJson());
    }
}
// ******
// ********************************

bool ChatPage::eventFilter(QObject *obj, QEvent *event) {
    //头像框上传事件
    if (obj == barAvatarLabel && event->type() == QEvent::MouseButtonRelease) {
        handleAvatarUpload();
        return true;
    }

    //同时拦截普通输入框和酒吧输入框的按键事件
    if ((obj == chatInput || obj == barChatInput) && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)) {
            if (keyEvent->modifiers() & Qt::ShiftModifier) {
                return false; //Shift+Enter 换行
            } else {
                //根据当前所在的输入框执行不同的发送逻辑
                if (obj == chatInput) {
                    //普通模式下原有逻辑是调用 sendBtn 的点击
                    sendBtn->animateClick();
                } else if (obj == barChatInput) {
                    handleBarSend();
                }
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void ChatPage::appendMessage(const QString &sender, const QString &msg, const QString &color) {
    Q_UNUSED(color);
    bool isUser = (sender == "Me" || sender == "用户" || sender == "我" || sender == "user");

    //根据当前模式选择目标浏览器
    QTextBrowser *targetBrowser = m_isBarMode ? barChatHistory : chatHistory;

    //如果是非流式的普通追加（如用户发送的消息）
    QString finalHtml = wrapInBubble(msg, isUser);
    targetBrowser->append(finalHtml);

    //自动滚动到底部
    targetBrowser->moveCursor(QTextCursor::End);
}

void ChatPage::appendSystemMsg(const QString &msg) {
    QTextBrowser *targetBrowser = m_isBarMode ? barChatHistory : chatHistory;
    targetBrowser->append(QString("<i style='color:gray; font-size:14px;'>系统: %1</i>").arg(msg));
}

///
/// \brief ChatPage::handleAvatarUpload
/// \brief 头像上传和持久化逻辑
///
void ChatPage::handleAvatarUpload()
{
    if (m_currentPresetName.isEmpty()) return;

    QString filePath = QFileDialog::getOpenFileName(this, "选择角色头像", "", "图片文件 (*.png *.jpg *.jpeg)");
    if (filePath.isEmpty()) return;

    // 1.设置持久化存储路径
    QString avatarDir = QDir(QCoreApplication::applicationDirPath()).filePath("sys/presets/avatars");
    QDir().mkpath(avatarDir);  //如果文件夹不存在则递归创建

    // 2.生成唯一文件名，确保不同预设间不共用头像
    //用了 UUID 保证即便用户上传了同名图片，在后台也会存为两个独立文件
    QString suffix = QFileInfo(filePath).suffix();
    QString fileName = QUuid::createUuid().toString(QUuid::WithoutBraces) + "." + suffix;
    QString targetPath = QDir(avatarDir).filePath(fileName);

    // 3. 物理拷贝并持久化
    if (QFile::copy(filePath, targetPath)) {
        //更新预设数据
        m_presets[m_currentPresetName].avatarPath = targetPath;

        //应用圆形渲染
        setCircularAvatar(barAvatarLabel, targetPath);

        //物理保存到该角色的 .json 配置文件中
        saveCurrentToBarPreset();
    }
}

///
/// \brief ChatPage::setCircularAvatar
/// \brief 头像渲染接口
/// \brief 保持头像上传后仍然是圆形效果，并可作为渲染接口给外界使用
/// \param label
/// \param path
///
void ChatPage::setCircularAvatar(QLabel* label, const QString& path)
{
    // 1.检查路径有效性。如果路径为空或文件不存在，显示默认样式
    if (path.isEmpty() || !QFile::exists(path)) {
        label->setText("无头像");
        //使用样式表实现一个简单的灰色圆圈背景
        label->setStyleSheet("background-color: #eee; border-radius: 40px; border: 2px solid white; color: #999;");
        return;
    }

    // 2.加载原始图片
    QPixmap src(path);
    if (src.isNull()) return;

    // 3.准备画布。创建一个和 Label 大小一致的 QPixmap，并填充透明色
    QSize size(label->width(), label->height());
    QPixmap target(size);
    target.fill(Qt::transparent);

    // 4.初始化画家 (QPainter)
    QPainter painter(&target);
    //开启抗锯齿，防止圆形边缘出现毛边
    painter.setRenderHint(QPainter::Antialiasing, true);
    //开启平滑缩放，保证图片缩小时不失真
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // 5.定义裁剪路径
    QPainterPath pathCircle;
    //在画布范围内画一个圆
    pathCircle.addEllipse(0, 0, size.width(), size.height());
    //告诉画家：接下来的绘制操作只在这个圆形路径内生效
    painter.setClipPath(pathCircle);

    // 6.绘制图片
    //将原图按“充满且裁剪” (KeepAspectRatioByExpanding) 的原则缩放到画布大小
    QPixmap scaledSrc = src.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    //将缩放后的图片画在画布中心
    painter.drawPixmap(0, 0, scaledSrc);

    // 7.结束绘制并应用到 UI
    painter.end();
    label->setPixmap(target);
    //移除之前的文字和背景色，仅保留白色圆边框视觉感
    label->setStyleSheet("border: 2px solid white; border-radius: 40px; background: transparent;");
}

// **************** 流式输出 ****************
///
/// \brief ChatPage::handleStreamingResponse
/// \brief 流式输出渲染接口
///
void ChatPage::handleStreamingResponse(const QString &text)
{
    QTextBrowser *targetBrowser = m_isBarMode ? barChatHistory : chatHistory;

    if(!m_isStreamingTyping)
    {
        m_isStreamingTyping = true;
        m_currentResponse = "";

        //记录流式输出开始时光标的确切位置
        QTextCursor cursor = targetBrowser->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_startPos = cursor.position();
    }

    m_currentResponse += text;

    //给文本包裹样式
    QString html = wrapInBubble(m_currentResponse + " ▌", false);  //加上Markdown渲染和打字机光标

    //设置键鼠光标
    QTextCursor cursor = targetBrowser->textCursor();

    //精准选中上一次渲染的整个气泡并替换
    cursor.setPosition(m_startPos);  //回到起笔位置
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);  //类同按住Shift来选中块，选中整个气泡
    cursor.removeSelectedText();  //删掉旧内容
    cursor.insertHtml(html);      //插入新内容

    targetBrowser->ensureCursorVisible();
}

///
/// \brief ChatPage::finishStreamingResponse
/// \brief 流式输出结束时渲染接口，对最终文本进行处理
/// \param fullMsg
///
void ChatPage::finishStreamingResponse(const QString &fullMsg)
{
    m_isStreamingTyping = false;
    QTextBrowser *targetBrowser = m_isBarMode ? barChatHistory : chatHistory;

    //移除打字机光标，进行最终渲染
    QString html = wrapInBubble(fullMsg, false);

    //设置键鼠光标
    QTextCursor cursor = targetBrowser->textCursor();

    cursor.setPosition(m_startPos);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.insertHtml(html);

    targetBrowser->ensureCursorVisible();
}
// ********************************

///
/// \brief ChatPage::wrapInBubble
/// \brief 将内容包装在气泡形状的HTML中
/// \param content
/// \param isUser
/// \return
///
QString ChatPage::wrapInBubble(const QString &rawContent, bool isUser) {
    //将传入的Markdown转为HTML
    QTextDocument doc;
    doc.setMarkdown(rawContent);
    QString htmlContent = doc.toHtml();

    //剥离<html>和<body>标签，提取内部的实际元素，防止Qt底层渲染引擎因为标签嵌套不合法而出现渲染问题
    int bodyStart = htmlContent.indexOf("<body");
    if(bodyStart != -1)
    {
        bodyStart = htmlContent.indexOf(">", bodyStart) + 1;
        int bodyEnd = htmlContent.lastIndexOf("</body>");
        if (bodyEnd != -1) {
            htmlContent = htmlContent.mid(bodyStart, bodyEnd - bodyStart);
        }
    }


    htmlContent.replace("color:#000000;", "");

    //配置样式
    QString bgColor = isUser ? "#0078d4" : "#f1f1f1";
    QString textColor = isUser ? "#ffffff" : "#202124";

    //返回“气泡”样式表
    if (isUser) {
        return QString(
                   "<table width='100%' border='0' cellpadding='0' cellspacing='0' style='margin-top: 8px;'>"
                   "  <tr>"
                   "    <td width='20%'></td>" // 左侧占位 20%，保证气泡最宽 80%
                   "    <td align='right'>"
                   "      <div style='background-color: %1; color: %2; padding: 10px 14px; "
                   "                  font-family: \"Microsoft YaHei\", sans-serif; font-size: 16px; "
                   "                  text-align: left;'>" // 气泡内部文字左对齐，方便阅读
                   "        %3"
                   "      </div>"
                   "    </td>"
                   "  </tr>"
                   "</table>"
                   ).arg(bgColor, textColor, htmlContent);
    } else {
        return QString(
                   "<table width='100%' border='0' cellpadding='0' cellspacing='0' style='margin-top: 8px;'>"
                   "  <tr>"
                   "    <td align='left'>"
                   "      <div style='background-color: %1; color: %2; padding: 10px 14px; "
                   "                  font-family: \"Microsoft YaHei\", sans-serif; font-size: 16px;'>"
                   "        %3"
                   "      </div>"
                   "    </td>"
                   "    <td width='20%'></td>" // 右侧占位 20%
                   "  </tr>"
                   "</table>"
                   ).arg(bgColor, textColor, htmlContent);
    }
}

// **************** 多对话机制 ****************
///
/// \brief ChatPage::rebuildChatFromHistory
/// \brief 多对话机制辅助函数：从历史记录重绘对话UI
/// \param history
///
void ChatPage::rebuildChatFromHistory(const QJsonArray &history) {
    //根据当前模式清空对应的历史框
    QTextBrowser *targetBrowser = m_isBarMode ? barChatHistory : chatHistory;
    targetBrowser->clear();

    for (const QJsonValue &value : history) {
        QJsonObject obj = value.toObject();
        QString role = obj["role"].toString();
        QString content = obj["content"].toString();

        if (role == "user")
        {
            appendMessage("我", content, "black");
        }
        else if (role == "assistant")
        {
            targetBrowser->append(wrapInBubble(content, false));
        }
        else if (role == "system" && content.startsWith("[之前的对话总结]"))
        {
            targetBrowser->append(QString("<i style='color:gray; font-size:14px;'>系统: %1</i>").arg(content));
        }
    }

    //如果历史记录不为空，说明不是第一句话了
    m_isFirstMessage = history.isEmpty();
}
// ********************************

// **************** 数据存储 ****************
///
/// \brief serializeNodeHelper
/// \brief 辅助函数：将树的节点和子节点转成JSON对象（递归）
/// \param item
/// \return
///
QJsonObject serializeNodeHelper(QTreeWidgetItem *item) {
    QJsonObject obj;
    obj.insert("text", item->text(0));
    obj.insert("type", item->data(0, Qt::UserRole).toString());     //chat 或 folder
    obj.insert("uuid", item->data(0, Qt::UserRole + 1).toString()); //会话ID

    QJsonArray children;
    for (int i = 0; i < item->childCount(); ++i) {
        children.append(serializeNodeHelper(item->child(i)));
    }
    obj.insert("children", children);
    return obj;
}

///
/// \brief ChatPage::serializeTree
/// \brief 将树转成JSON
/// \return
///
QJsonArray ChatPage::serializeTree() {
    QJsonArray root;
    for (int i = 0; i < chatSessionsTree->topLevelItemCount(); ++i) {
        root.append(serializeNodeHelper(chatSessionsTree->topLevelItem(i)));
    }
    return root;
}

///
/// \brief ChatPage::deserializeTree
/// \brief 将JSON恢复成树
/// \param data
///
void ChatPage::deserializeTree(const QJsonArray &data) {
    chatSessionsTree->clear();

    //递归恢复节点
    std::function<void(const QJsonArray&, QTreeWidgetItem*)> parse =
        [&](const QJsonArray &nodeList, QTreeWidgetItem *parent) {
            for (const QJsonValue &value : nodeList) {
                QJsonObject obj = value.toObject();
                QTreeWidgetItem *item = new QTreeWidgetItem();
                item->setText(0, obj["text"].toString());
                item->setData(0, Qt::UserRole, obj["type"].toString());
                item->setData(0, Qt::UserRole + 1, obj["uuid"].toString());

                //恢复可编辑等属性
                item->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);

                if (parent) parent->addChild(item);
                else chatSessionsTree->addTopLevelItem(item);

                parse(obj["children"].toArray(), item);
            }
        };
    parse(data, nullptr);
}
// ********************************

// ========================== 弹窗实现区 ==========================

TalkExampleDialog::TalkExampleDialog(const QList<QPair<QString, QString>> &examples, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("辅助对话设定");
    resize(550, 600);
    setStyleSheet("QDialog { background: #fafafa; }");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QHBoxLayout *header = new QHBoxLayout();
    QLabel *titleLabel = new QLabel("<b>辅助对话</b>");
    titleLabel->setStyleSheet("font-size: 15px; color: #333;");
    header->addWidget(titleLabel);

    countLabel = new QLabel("(共 0 条)");
    countLabel->setStyleSheet("color: #999; font-size: 12px;");
    header->addWidget(countLabel);
    header->addStretch();
    mainLayout->addLayout(header);

    //滚动区域
    QScrollArea *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("background: transparent;");
    QWidget *scrollContent = new QWidget();
    listLayout = new QVBoxLayout(scrollContent);
    listLayout->setAlignment(Qt::AlignTop);
    scroll->setWidget(scrollContent);
    mainLayout->addWidget(scroll);

    QPushButton *addBtn = new QPushButton("+ 添加对话条目");
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setStyleSheet(
        "QPushButton { background: #fdf2f8; color: #db2777; border: 1px dashed #f9a8d4; padding: 10px; border-radius: 6px; font-weight: bold; margin-top: 5px; }"
        "QPushButton:hover { background: #fce7f3; border-color: #f472b6; }"
        );
    connect(addBtn, &QPushButton::clicked, this, [this](){ addItem(); });
    mainLayout->addWidget(addBtn);

    //底部按钮
    QDialogButtonBox *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    btnBox->button(QDialogButtonBox::Ok)->setText("保存");
    btnBox->button(QDialogButtonBox::Cancel)->setText("取消");

    btnBox->setStyleSheet(
        "QPushButton { padding: 6px 15px; border-radius: 4px; font-size: 13px; font-weight: bold; outline: none; }"
        "QPushButton[text=\"保存\"] { background: #db2777; color: white; border: none; }"
        "QPushButton[text=\"保存\"]:hover { background: #be185d; }"
        "QPushButton[text=\"取消\"] { background: white; color: #333; border: 1px solid #ccc; }"
        "QPushButton[text=\"取消\"]:hover { background: #f0f0f0; }"
        );
    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(btnBox);

    //渲染传入的现有数据
    for (const auto &pair : examples) {
        addItem(pair.first, pair.second);
    }
    updateCount();
}

void TalkExampleDialog::addItem(const QString &u, const QString &a) {
    QFrame *frame = new QFrame();

    frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed); //水平可拉伸，垂直不可拉伸
    frame->setStyleSheet(
        "QFrame { background: #ffffff; border: 1px solid #e1e4e8; border-radius: 10px; margin-bottom: 5px; }"
        "QFrame:hover { border-color: #f9a8d4; }"
        );

    QVBoxLayout *frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(12, 12, 12, 12);
    frameLayout->setSpacing(8);

    //删除按钮
    QHBoxLayout *frameHeader = new QHBoxLayout();
    frameHeader->setContentsMargins(0, 0, 0, 0);
    QPushButton *delBtn = new QPushButton("×");
    delBtn->setFixedSize(22, 22);
    delBtn->setCursor(Qt::PointingHandCursor);
    delBtn->setToolTip("删除此条示例");
    delBtn->setStyleSheet(
        "QPushButton { color: #aaa; border: none; font-size: 18px; font-weight: bold; background: transparent; }"
        "QPushButton:hover { color: #ef4444; background: #fee2e2; border-radius: 11px; }"
        );
    frameHeader->addStretch();
    frameHeader->addWidget(delBtn);
    frameLayout->addLayout(frameHeader);

    //问
    QHBoxLayout *uLayout = new QHBoxLayout();
    QLabel *uLabel = new QLabel("问:");
    uLabel->setStyleSheet("color: #0078d4; font-weight: bold; font-size: 13px; border: none; background: transparent;");
    QPlainTextEdit *uEdit = new QPlainTextEdit(u);
    uEdit->setPlaceholderText("用户可能会问的话...");
    uEdit->setFixedHeight(45);
    uEdit->setStyleSheet(
        "QPlainTextEdit { border: 1px solid #f0f0f0; background: #f8faff; border-radius: 6px; padding: 4px; color: #333; }"
        "QPlainTextEdit:focus { border: 1px solid #0078d4; }"
        );
    uLayout->addWidget(uLabel, 0, Qt::AlignTop);
    uLayout->addWidget(uEdit, 1);
    frameLayout->addLayout(uLayout);

    //答
    QHBoxLayout *aLayout = new QHBoxLayout();
    QLabel *aLabel = new QLabel("答:");
    aLabel->setStyleSheet("color: #db2777; font-weight: bold; font-size: 13px; border: none; background: transparent;");
    QPlainTextEdit *aEdit = new QPlainTextEdit(a);
    aEdit->setPlaceholderText("期望 AI 回复的内容...");
    aEdit->setFixedHeight(75);
    aEdit->setStyleSheet(
        "QPlainTextEdit { border: 1px solid #f0f0f0; background: #fff9fb; border-radius: 6px; padding: 4px; color: #333; }"
        "QPlainTextEdit:focus { border: 1px solid #db2777; }"
        );
    aLayout->addWidget(aLabel, 0, Qt::AlignTop);
    aLayout->addWidget(aEdit, 1);
    frameLayout->addLayout(aLayout);

    listLayout->addWidget(frame);
    m_items.append({ frame, uEdit, aEdit });
    updateCount();

    connect(delBtn, &QPushButton::clicked, [this, frame]() {
        for(int i=0; i < m_items.size(); ++i) {
            if(m_items[i].container == frame) {
                m_items.removeAt(i);
                break;
            }
        }
        frame->deleteLater();
        updateCount();
    });
}

void TalkExampleDialog::updateCount() {
    countLabel->setText(QString("(共 %1 条)").arg(m_items.size()));
}

QList<QPair<QString, QString>> TalkExampleDialog::getExamples() const {
    QList<QPair<QString, QString>> result;
    for (const auto &item : m_items) {
        QString uText = item.uEdit->toPlainText().trimmed();
        QString aText = item.aEdit->toPlainText().trimmed();
        if (!uText.isEmpty() || !aText.isEmpty()) {
            result.append({uText, aText});
        }
    }
    return result;
}

KnowledgeLibraryDialog::KnowledgeLibraryDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("知识库设定");
    resize(400, 300);
    setStyleSheet("QDialog { background: white; }");

    QVBoxLayout *layout = new QVBoxLayout(this);
    QLabel *label = new QLabel("📚 知识库功能开发中，敬请期待...\n(RAG / Vector Database)\n\n目前仅为空界面");
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("color: #666; font-size: 14px;");
    layout->addWidget(label);

    QDialogButtonBox *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok);
    btnBox->button(QDialogButtonBox::Ok)->setText("知道了");
    btnBox->button(QDialogButtonBox::Ok)->setStyleSheet(
        "background: #db2777; color: white; border: none; padding: 6px 15px; border-radius: 4px; outline: none;"
        );
    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    layout->addWidget(btnBox);
}
// ====================================================