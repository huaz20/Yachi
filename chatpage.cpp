#include "chatpage.h"
#include <QEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTextDocument>
#include <QUuid>
#include <QJsonArray>
#include <QJsonObject>

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
    mainLayout->addWidget(rightAreaContainer, 1); // 占比 1，撑满剩余空间

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

    // 顶部的收起按钮栏
    QHBoxLayout *topCollapseLayout = new QHBoxLayout();
    topCollapseLayout->setContentsMargins(5, 5, 5, 0); // 微调边距

    QPushButton *collapseBtn = new QPushButton("<"); // 收回符号
    collapseBtn->setFixedSize(30, 30);
    collapseBtn->setCursor(Qt::PointingHandCursor);
    collapseBtn->setToolTip("收起侧边栏");
    // 设置无边框、悬停变灰色的扁平圆按钮样式
    collapseBtn->setStyleSheet("QPushButton { background: transparent; border: none; border-radius: 15px; font-size: 18px; font-weight: bold; color: #666; }"
                               "QPushButton:hover { background-color: #e2e6ea; color: #000; }");

    topCollapseLayout->addWidget(collapseBtn);
    topCollapseLayout->addStretch(); // 添加弹簧，把按钮推到最左侧

    expandedLayout->addLayout(topCollapseLayout); // 将顶部栏加到展开布局的最上方

    // 树状结构（使对话可以放在文件夹里）
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

    // “最近对话”分割线装饰
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

        // 读取我们在创建节点时打上的暗号
        QString type = item->data(0, Qt::UserRole).toString();
        if (type.isEmpty()) return;

        QString text = item->text(0);

        // 关键：暂时屏蔽信号，防止在下面 setText 时触发死循环
        chatSessionsTree->blockSignals(true);

        // 清理用户输入时可能残留的旧图标，防止出现 "📁 📁 名字" 的情况
        text.remove("📁");
        text.remove("💬");
        text.remove("📝");
        text = text.trimmed();

        // 根据不同类型，强行加上专属图标
        if (type == "folder") {
            item->setText(0, "📁 " + text);
        } else if (type == "chat") {
            item->setText(0, "💬 " + text);
        }

        // 恢复信号
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

    chatInput = new QTextEdit();
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

    connect(barModeBtn, &QPushButton::clicked, [this]() {
        stackedWidget->setCurrentWidget(barWidget);
        appendSystemMsg("已进入酒吧模式");
    });
}

// **************** 酒吧模式 ****************
///
/// \brief ChatPage::setupBarModeUI
/// \brief UI层构建
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
    barChatInput = new QTextEdit();
    barChatInput->setPlaceholderText("想对她说点什么...");
    barChatInput->setStyleSheet("font-size: 15px;");  //14px（默认）+1
    barChatInput->setFrameShape(QFrame::NoFrame);
    chatLayout->addWidget(barChatInput,2);

    mainBarLayout->addWidget(chatContainer, 1);

    // ================== 右侧：设定面板 (3:2 比例) ==================
    barSidePanel = new QWidget();
    barSidePanel->setFixedWidth(320);
    barSidePanel->setStyleSheet("background-color: #fafafa; border-left: 1px solid #eee;");
    QVBoxLayout *sideLayout = new QVBoxLayout(barSidePanel);

    // 1.预设与头像
    presetCombo = new QComboBox();
    presetCombo->addItems({"月见八百代", "高冷御姐", "元气少女"});
    sideLayout->addWidget(presetCombo);

    barAvatarLabel = new QLabel("上传头像");
    barAvatarLabel->setFixedSize(80, 80);
    barAvatarLabel->setStyleSheet("background-color: #eee; border-radius: 40px; border: 2px solid white;");
    sideLayout->addWidget(barAvatarLabel, 0, Qt::AlignCenter);

    // 2.人格设定 (伸缩权重 3)
    QWidget *pArea = new QWidget();
    QVBoxLayout *pLayout = new QVBoxLayout(pArea);
    pLayout->addWidget(new QLabel("<b>人格设定</b>"));
    barPersonaEdit = new QTextEdit();
    barPersonaEdit->setStyleSheet("border: 1px solid #ddd; border-radius: 8px; background: white;");
    pLayout->addWidget(barPersonaEdit);
    sideLayout->addWidget(pArea, 3);

    // 3.辅助对话 (伸缩权重 2)
    QWidget *eArea = new QWidget();
    QVBoxLayout *eLayout = new QVBoxLayout(eArea);

    QHBoxLayout *eHeader = new QHBoxLayout();
    eHeader->addWidget(new QLabel("<b>辅助对话</b>"));
    exampleCountLabel = new QLabel("(共 0 条)");
    exampleCountLabel->setStyleSheet("color: #999; font-size: 11px;");
    eHeader->addWidget(exampleCountLabel);
    eHeader->addStretch();
    eLayout->addLayout(eHeader);

    QScrollArea *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    QWidget *scrollContent = new QWidget();
    talkExamplesLayout = new QVBoxLayout(scrollContent);
    talkExamplesLayout->setAlignment(Qt::AlignTop);
    scroll->setWidget(scrollContent);
    eLayout->addWidget(scroll);

    QPushButton *addBtn = new QPushButton("+ 添加条目");
    addBtn->setStyleSheet("background: #fdf2f8; color: #db2777; border: 1px dashed #f9a8d4;");
    eLayout->addWidget(addBtn);
    sideLayout->addWidget(eArea, 2);

    mainBarLayout->addWidget(barSidePanel);
    stackedWidget->addWidget(barWidget);

    // ================== 信号绑定 ==================
    connect(exitBarModeBtn, &QPushButton::clicked, [this](){
        stackedWidget->setCurrentWidget(normalWidget);
    });

    //连接右侧面板折叠逻辑
    connect(toggleBarSideBtn, &QPushButton::toggled, [this](bool checked){
        barSidePanel->setVisible(checked);
        toggleBarSideBtn->setText(checked ? "收起 ⚙️" : "设定 ⚙️");
    });

    //示例对话添加按钮
    connect(addBtn, &QPushButton::clicked, [this](){ addTalkExampleItem(); });
}

///
/// \brief ChatPage::addTalkExampleItem
/// \brief 辅助函数：增加示例对话条目
/// \param userText
/// \param aiText
///
void ChatPage::addTalkExampleItem(const QString &userText, const QString &aiText) {
    QFrame *frame = new QFrame();
    frame->setStyleSheet("QFrame { background: #fff; border: 1px solid #eee; border-radius: 6px; margin-bottom: 5px; }");
    QVBoxLayout *layout = new QVBoxLayout(frame);

    QTextEdit *uEdit = new QTextEdit(userText);
    uEdit->setPlaceholderText("用户提问...");
    uEdit->setFixedHeight(35);
    uEdit->setStyleSheet("border: none; background: #f9f9f9;");

    QTextEdit *aEdit = new QTextEdit(aiText);
    aEdit->setPlaceholderText("AI 回复...");
    aEdit->setFixedHeight(35);
    aEdit->setStyleSheet("border: none; background: #fdf2f8;");

    QPushButton *delBtn = new QPushButton("×");
    delBtn->setFixedSize(20, 20);
    delBtn->setStyleSheet("color: #ccc; border: none; font-weight: bold;");

    QHBoxLayout *h = new QHBoxLayout();
    h->addStretch();
    h->addWidget(delBtn);
    layout->addLayout(h);
    layout->addWidget(uEdit);
    layout->addWidget(aEdit);

    talkExamplesLayout->addWidget(frame);
    m_talkExamples.append({ frame, uEdit, aEdit });
    updateExampleCount();

    connect(delBtn, &QPushButton::clicked, [this, frame]() {
        for(int i=0; i<m_talkExamples.size(); ++i) {
            if(m_talkExamples[i].container == frame) {
                m_talkExamples.removeAt(i);
                break;
            }
        }
        frame->deleteLater();
        updateExampleCount();
    });
}

///
/// \brief ChatPage::updateExampleCount
/// \brief 辅助函数：更新示例对话条目计数标签
///
void ChatPage::updateExampleCount() {
    if(exampleCountLabel) exampleCountLabel->setText(QString("(共 %1 条)").arg(m_talkExamples.size()));
}
// ********************************

bool ChatPage::eventFilter(QObject *obj, QEvent *event) {
    if (obj == chatInput && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)) {
            if (keyEvent->modifiers() & Qt::ShiftModifier) {
                return false; //Shift+Enter换行
            } else {
                sendBtn->animateClick();
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void ChatPage::appendMessage(const QString &sender, const QString &msg, const QString &color) {
    Q_UNUSED(color);
    bool isUser = (sender == "Me" || sender == "用户" || sender == "我" || sender == "user");

    //如果是非流式的普通追加（如用户发送的消息）
    QString finalHtml = wrapInBubble(msg, isUser);
    chatHistory->append(finalHtml);

    //自动滚动到底部
    chatHistory->moveCursor(QTextCursor::End);
}

void ChatPage::appendSystemMsg(const QString &msg) {
    chatHistory->append(QString("<i style='color:gray; font-size:14px;'>系统: %1</i>").arg(msg));
}

// **************** 流式输出 ****************
///
/// \brief ChatPage::handleStreamingResponse
/// \brief 流式输出渲染接口
///
void ChatPage::handleStreamingResponse(const QString &text)
{
    if(!m_isStreamingTyping)
    {
        m_isStreamingTyping = true;
        m_currentResponse = "";

        //记录流式输出开始时光标的确切位置
        QTextCursor cursor = chatHistory->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_startPos = cursor.position();
    }

    m_currentResponse += text;

    //给文本包裹样式
    QString html = wrapInBubble(m_currentResponse + " ▌", false);  //加上Markdown渲染和打字机光标

    //设置键鼠光标
    QTextCursor cursor = chatHistory->textCursor();

    //精准选中上一次渲染的整个气泡并替换
    cursor.setPosition(m_startPos);  //回到起笔位置
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);  //类同按住Shift来选中块，选中整个气泡
    cursor.removeSelectedText();  //删掉旧内容
    cursor.insertHtml(html);      //插入新内容

    chatHistory->ensureCursorVisible();
}

///
/// \brief ChatPage::finishStreamingResponse
/// \brief 流式输出结束时渲染接口，对最终文本进行处理
/// \param fullMsg
///
void ChatPage::finishStreamingResponse(const QString &fullMsg)
{
    m_isStreamingTyping = false;

    //移除打字机光标，进行最终渲染
    QString html = wrapInBubble(fullMsg, false);

    //设置键鼠光标
    QTextCursor cursor = chatHistory->textCursor();

    cursor.setPosition(m_startPos);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.insertHtml(html);

    chatHistory->ensureCursorVisible();
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
    chatHistory->clear();

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
            chatHistory->append(wrapInBubble(content, false));
        }
        else if (role == "system" && content.startsWith("[之前的对话总结]"))
        {
            appendSystemMsg(content);
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