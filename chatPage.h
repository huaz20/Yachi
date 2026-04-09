#ifndef CHATPAGE_H
#define CHATPAGE_H

#include <QWidget>
#include <QTextEdit>
#include <QTextBrowser> // 新增引入
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QLabel>
#include <QKeyEvent>
#include <QTreeWidget>
#include <QMenu>

class ChatPage : public QWidget {
    Q_OBJECT
public:
    explicit ChatPage(QWidget *parent = nullptr);
    void appendMessage(const QString &sender, const QString &msg, const QString &color);
    void appendSystemMsg(const QString &msg);

signals:
    void sendMessage(const QString &text);
    void requestClearHistory(); // 通知底层清空当前 Agent 的记忆
    void requestTitleSummary(const QString &firstMessage); // 请求 AI 生成标题

public slots:
    void updateCurrentChatTitle(const QString &title); // 供外部调用，更新当前列表项的标题

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void setupSidebar();
    void setupNormalChatUI();
    void setupBarModeUI();

    void toggleSidebar(bool expand);
    void showContextMenu(const QPoint &pos); // 处理右键菜单
    bool confirmDeletion(const QString &targetName); // 自定义无声确认弹窗

    bool m_isFirstMessage = true; // 判断是否是当前对话的第一句话

    // --- 核心布局控件 ---
    QStackedWidget *stackedWidget;

    // --- 侧边栏控件 ---
    QWidget *sidebarContainer;
    QWidget *collapsedSidebar;
    QWidget *expandedSidebar;
    QTreeWidget *chatSessionsTree; // 支持拖拽和层级的树状列表

    // --- 普通模式 UI 控件 ---
    QWidget *normalWidget;
    QTextBrowser *chatHistory; // 已修改为 QTextBrowser
    QTextEdit *chatInput;
    QPushButton *sendBtn;
    QPushButton *barModeBtn;

    // --- 酒吧模式 UI 控件 ---
    QWidget *barWidget;
    QPushButton *exitBarModeBtn;
};

#endif // CHATPAGE_H