#include "chatpage.h"
#include <QEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTextDocument> // 新增引入，用于解析 Markdown

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
    connect(chatSessionsTree, &QTreeWidget::customContextMenuRequested, this, &ChatPage::showContextMenu);

    auto createNewChat = [this]() {
        chatHistory->clear();
        m_isFirstMessage = true;

        QTreeWidgetItem *chat = new QTreeWidgetItem();
        chat->setText(0, "📝 新的聊天");

        // 打上属于“对话”的暗号
        chat->setData(0, Qt::UserRole, "chat");

        chat->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled);
        chatSessionsTree->insertTopLevelItem(0, chat);
        chatSessionsTree->setCurrentItem(chat);

        emit requestClearHistory();
        if (expandedSidebar->isHidden()) toggleSidebar(true);
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
            delete item; // 自动从树中移除并释放内存
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

    // 替换为 QTextBrowser，支持外链跳转
    chatHistory = new QTextBrowser();
    chatHistory->setReadOnly(true);
    chatHistory->setOpenExternalLinks(true);

    // 强制重置底层组件字号
    QFont baseFont = chatHistory->font();
    baseFont.setPixelSize(16);
    chatHistory->setFont(baseFont);

    // 设置基础样式表：控制 Markdown 的标题、代码块和引用样式
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
        if (!text.isEmpty()) {

            // 容错：如果列表空了，自动建一个新对话
            if (chatSessionsTree->topLevelItemCount() == 0 || chatSessionsTree->currentItem() == nullptr) {
                QTreeWidgetItem *chat = new QTreeWidgetItem();
                chat->setText(0, "📝 新的聊天");
                chat->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled);
                chatSessionsTree->insertTopLevelItem(0, chat);
                chatSessionsTree->setCurrentItem(chat);
                m_isFirstMessage = true;
            }

            emit sendMessage(text);
            chatInput->clear();

            // 首句触发 AI 起名
            if (m_isFirstMessage) {
                emit requestTitleSummary(text);
                m_isFirstMessage = false;
            }
        }
    };
    connect(sendBtn, &QPushButton::clicked, sendFunc);

    connect(barModeBtn, &QPushButton::clicked, [this]() {
        stackedWidget->setCurrentWidget(barWidget);
        appendSystemMsg("已进入酒吧模式");
    });
}

void ChatPage::setupBarModeUI() {
    barWidget = new QWidget();
    barWidget->setStyleSheet("background-color: #2b2b2b; color: white;");
    QVBoxLayout *layout = new QVBoxLayout(barWidget);

    QLabel *title = new QLabel("🍻 欢迎来到酒吧模式");
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 24px; font-weight: bold;");

    exitBarModeBtn = new QPushButton("退出");
    exitBarModeBtn->setStyleSheet("background-color: #555; padding: 10px;");

    layout->addStretch();
    layout->addWidget(title);
    layout->addWidget(exitBarModeBtn, 0, Qt::AlignCenter);
    layout->addStretch();

    stackedWidget->addWidget(barWidget);

    connect(exitBarModeBtn, &QPushButton::clicked, [this]() {
        stackedWidget->setCurrentWidget(normalWidget);
    });
}

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
    QTextDocument doc;

    // 强制重置解析器的默认字号，防止它覆盖我们的内联样式
    QFont font = chatHistory->font();
    font.setPixelSize(16);
    doc.setDefaultFont(font);

    doc.setMarkdown(msg);
    QString markdownHtml = doc.toHtml();

    // 拼装最终显示的 HTML
    QString htmlStr = QString("<div style='margin-bottom: 12px;'>"
                              "<b style='color:%1; font-size:16px;'>%2:</b>"
                              "<div style='margin-top: 4px; font-size:16px;'>%3</div>"
                              "</div>")
                          .arg(color, sender, markdownHtml);

    chatHistory->append(htmlStr);
}

void ChatPage::appendSystemMsg(const QString &msg) {
    chatHistory->append(QString("<i style='color:gray; font-size:14px;'>系统: %1</i>").arg(msg));
}