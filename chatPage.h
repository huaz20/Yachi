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
    //处理流式输出的增量文本
    void handleStreamingResponse(const QString &text);
    //结束流式输出并进行最终渲染
    void finishStreamingResponse(const QString &fullMsg);

    explicit ChatPage(QWidget *parent = nullptr);
    void appendMessage(const QString &sender, const QString &msg, const QString &color);
    void appendSystemMsg(const QString &msg);

    //多对话机制辅助函数：从历史记录重绘对话UI
    void rebuildChatFromHistory(const QJsonArray &history);

    // --- 数据存储 ---
    //树转JSON
    QJsonArray serializeTree();
    //JSON恢复树
    void deserializeTree(const QJsonArray &data);
    // ------


signals:
    void sendMessage(const QString &text);
    void requestClearHistory();                            //通知底层清空当前Agent的记忆
    void requestTitleSummary(const QString &firstMessage); //请求AI生成标题

    void chatSessionChanged(const QString &sessionId);     //当用户点击不同的对话项时触发
    void sessionDeleted(const QString &sessionId);         //UI层删除会话时传递的信号

public slots:
    void updateCurrentChatTitle(const QString &title);  //供外部调用，更新当前列表项的标题

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void setupSidebar();
    void setupNormalChatUI();
    void setupBarModeUI();

    void toggleSidebar(bool expand);
    void showContextMenu(const QPoint &pos); //处理右键菜单
    bool confirmDeletion(const QString &targetName); //删除确认

    //将内容包装在气泡形状的HTML中
    QString wrapInBubble(const QString &content, bool isUser);

    bool m_isFirstMessage = true; //判断是否是当前对话的第一句话

    // --- 核心布局控件 ---
    QStackedWidget *stackedWidget;
    QString m_currentResponse;  //暂存当前AI正在输出的文本
    bool m_isStreamingTyping = false;   //标记是否处于流式状态
    int m_startPos = 0;  //用来记录流式输出起点的光标位置

    // --- 侧边栏控件 ---
    QWidget *sidebarContainer;
    QWidget *collapsedSidebar;
    QWidget *expandedSidebar;
    QTreeWidget *chatSessionsTree; //支持拖拽和层级的树状列表

    // --- 普通模式UI ---
    QWidget *normalWidget;
    QTextBrowser *chatHistory;
    QTextEdit *chatInput;
    QPushButton *sendBtn;
    QPushButton *barModeBtn;

    // --- 酒吧模式UI ---
    QWidget *barWidget;
    QPushButton *exitBarModeBtn;
};

#endif // CHATPAGE_H