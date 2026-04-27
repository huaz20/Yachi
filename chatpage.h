#ifndef CHATPAGE_H
#define CHATPAGE_H

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QTextBrowser>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QLabel>
#include <QKeyEvent>
#include <QTreeWidget>
#include <QMenu>
#include <QComboBox>
#include <QScrollArea>
#include <QJsonArray>

///
/// \brief The BarPreset class
/// \brief 酒吧预设（角色卡预设）
///
struct BarPreset {
    QString name;
    QString avatarPath;
    QString persona;
    QList<QPair<QString, QString>> talkExamples; //用户/AI对话对
    QString kbPath; //知识库路径
};

///
/// \brief The TalkExamplePair class
/// \brief 用户/AI对话对
///
struct TalkExamplePair {
    QWidget *container;
    QTextEdit *userEdit;
    QTextEdit *aiEdit;
};

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

    //获取普通模式树数据
    QJsonArray getNormalTreeData() const {
        return m_isBarMode ? m_normalTreeData : const_cast<ChatPage*>(this)->serializeTree();
    }
    //获取酒吧模式树数据
    QJsonArray getBarTreeData() const {
        return m_isBarMode ? const_cast<ChatPage*>(this)->serializeTree() : m_barTreeData;
    }
    //设置数据
    void setNormalTreeData(const QJsonArray &data) { m_normalTreeData = data; }
    void setBarTreeData(const QJsonArray &data) { m_barTreeData = data; }
    // ------

    bool isBarMode() const { return m_isBarMode; }
    //组装酒吧模式的最终 System Prompt
    QString getBarSystemPrompt() const;

signals:
    void sendMessage(const QString &text);
    void requestClearHistory();                            //通知底层清空当前Agent的记忆
    void requestTitleSummary(const QString &firstMessage); //请求AI生成标题

    void chatSessionChanged(const QString &sessionId);     //当用户点击不同的对话项时触发
    void sessionDeleted(const QString &sessionId);         //UI层删除会话时传递的信号

    //酒吧模式
    void modeChanged(bool isBarMode);
    void barSendMessage(const QString &text);
    /* 酒吧模式逻辑层说明
     * 1、数据隔离：普通对话存储在 sys/normal，酒吧对话存储在 sys/bar
     * 2、树结构分离：有 m_normalTreeData 和 m_barTreeData 两个快照，在模式切换时通过序列化接口（serializeTree和deserializeTree）去切换。
     * 3、实时更新Prompt：酒吧模式下，每次按回车发送时，MainWindow 都会调用 getBarSystemPrompt() 重新抓取UI右侧的人格设定和对话示例，确保AI始终遵循最新的角色卡。
     * 4、AI的回复流向：handleStreamingResponse 内部根据 m_isBarMode 来选择 barChatHistory 或 chatHistory 进行打字机效果渲染。
     */

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
    QJsonArray m_normalTreeData;   //存储普通模式树结构
    QJsonArray m_barTreeData;      //存储酒吧模式树结构

    // --- 普通模式UI ---
    QWidget *normalWidget;
    QTextBrowser *chatHistory;
    QTextEdit *chatInput;
    QPushButton *sendBtn;
    QPushButton *barModeBtn;

    // --- 酒吧模式 ---
    bool m_isBarMode = false;
    //UI
    QWidget *barWidget;              //酒吧模式进入条
    QPushButton *exitBarModeBtn;     //酒吧模式退出按钮

    QWidget *barSidePanel;           //右侧设定面板容器
    QPushButton *toggleBarSideBtn;   //控制右侧面板收缩的按钮
    QLineEdit *barCharNameEdit;      // 可修改的角色名称
    QLabel *barAvatarLabel;          //酒吧模式圆形头像框
    QTextEdit *barPersonaEdit;       //人格设定
    QComboBox *presetCombo;          //预设下拉框声明

    QLabel *exampleCountLabel;       //示例对话计数标签
    QVBoxLayout *talkExamplesLayout; //对话条目的布局
    QList<TalkExamplePair> m_talkExamples;

    QWidget *personaContainer;       //人格设定容器
    QWidget *examplesContainer;      //辅助对话容器

    QTextBrowser *barChatHistory;    //聊天记录
    QTextEdit *barChatInput;         //输入框

    QWidget *barChatToolbar;         //酒吧模式工具栏容器
    QPushButton *btnEmoji;           //表情
    QPushButton *btnScreenShot;      //截图
    QPushButton *btnFile;            //文件
    QPushButton *btnVibrate;         //窗口抖动

    //辅助函数
    void addTalkExampleItem(const QString &userText = "", const QString &aiText = "");  //添加示例对话条目
    void clearTalkExamples();                 //清空示例对话条目
    void loadBarPreset(const QString &name);  //加载酒吧预设
    void saveCurrentToBarPreset();            //保存当前酒吧预设
    void updateExampleCount();                //更新计数显示

    //主逻辑
    void handleBarSend();             //处理酒吧模式的发送逻辑
    // ------
};

#endif // CHATPAGE_H
