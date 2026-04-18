#ifndef VOICEPAGE_H
#define VOICEPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QProgressBar>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QScrollArea>
#include <QTabWidget>
#include <QFrame>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QBuffer>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class VoicePage : public QWidget {
    Q_OBJECT
public:
    explicit VoicePage(QWidget *parent = nullptr);

private slots:
    void onGenerateClicked();
    void onPlayClicked();
    void onExportClicked();
    void onBrowseCachePath();     //浏览缓存目录
    void handleFinished();
    void updatePlaybackProgress();

    // --- 预设管理槽函数 ---
    void loadSelectedPreset();
    void addNewPreset();
    void renamePreset();
    void deletePreset();
    void saveCurrentPreset();
    void refreshPresetList();

    // --- 训练配置弹窗槽函数 ---
    void openTrainingConfigDialog();

private:
    void setupUI();
    void initAudio();

    //辅助函数
    QHBoxLayout* createSliderWithSpinBox(const QString &label, double min, double max, double step, double defaultVal, QSlider*& outSlider, QDoubleSpinBox*& outSpin, int decimals = 2);
    QHBoxLayout* createIntSliderWithSpinBox(const QString &label, int min, int max, int defaultVal, QSlider*& outSlider, QSpinBox*& outSpin);
    void applyCardShadow(QWidget *widget);

    // ==========================================
    // 1. 预设管理区控件
    // ==========================================
    QComboBox *presetCombo;
    QPushButton *addPresetBtn;
    QPushButton *renamePresetBtn;
    QPushButton *deletePresetBtn;
    QPushButton *savePresetBtn;

    // ==========================================
    // 2. 参数配置区控件
    // ==========================================
    QTabWidget *configTabs;

    QSlider *speedSlider;    QDoubleSpinBox *speedSpin;
    QSlider *pitchSlider;    QDoubleSpinBox *pitchSpin;
    QSlider *intervalSlider; QDoubleSpinBox *intervalSpin;

    QSlider *tempSlider;     QDoubleSpinBox *tempSpin;
    QSlider *topPSlider;     QDoubleSpinBox *topPSpin;
    QSlider *topKSlider;     QSpinBox *topKSpin;

    QComboBox *splitCombo;
    QSpinBox *batchSpin;

    QCheckBox *useLlmEmotionCheck;
    QComboBox *llmModelCombo;

    QGroupBox *cacheGroup;
    QSpinBox *cacheExpireSpin;
    QLineEdit *cachePathEdit;

    // ==========================================
    // 3. 音频模型配置区控件
    // ==========================================
    QTabWidget *modelTabs;
    QLineEdit *refAudioPathEdit;

    QLineEdit *ckptPathEdit;
    QLineEdit *pthPathEdit;
    QLineEdit *refTextEdit;
    QComboBox *refLangCombo;

    QTextEdit *trainLogConsole;   //训练日志控制台

    // ==========================================
    // 4. 文本输出区控件
    // ==========================================
    QTextEdit *inputText;
    QPushButton *generateBtn;
    QPushButton *playBtn;
    QComboBox *exportFormatCombo;
    QPushButton *exportBtn;
    QProgressBar *progressBar;

    // ==========================================
    // 多媒体与网络
    // ==========================================
    QMediaPlayer *mediaPlayer;
    QAudioOutput *audioOutput;
    QNetworkAccessManager *networkManager;
    QBuffer *audioBuffer;
    QByteArray m_audioData;
};

#endif // VOICEPAGE_H