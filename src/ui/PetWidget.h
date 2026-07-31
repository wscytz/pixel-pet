#pragma once

#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <string>

#include "ui/BeadGrid.h"
#include "ui/PetAnimator.h"
#include "ui/PetScene.h"   // EqStyle(音浪样式成员)
#include "audio/AudioEngine.h"
#include "audio/WebAudioSource.h"
#include "audio/SystemAudioSource.h"
#include "dsp/FeatureExtractor.h"
#include "emotion/Emotion.h"
#include "ui/FocusStats.h"

class QSystemTrayIcon;
class QEvent;

class PetWidget : public QWidget {
    Q_OBJECT
public:
    explicit PetWidget(QWidget* parent = nullptr);

    QSize sizeHint() const override { return {side_, side_}; }

    void setEmotion(const Emotion& e);
    void setGenre(Genre g) { anim_.setGenre(g); }
    void setSpectrum(const float* spec, int n);
    void snapEmotion(const Emotion& e, Genre g) { anim_.snap(e, g); }
    std::string gridAscii() const;
    void loadFile(const QString& path);

    // 音源状态/控制(设置页用)
    bool webEnabled() const { return webEnabled_; }
    void setWebEnabled(bool b);
    bool webConnected() const;        // 网页扩展连接状态(状态点)
    bool systemOn() const;            // 系统音频是否在抓
    void startSystemAudio();
    void stopSystemAudio();
    bool enginePlaying() const;       // 本地文件在播
    QString currentSourceName() const;   // 当前驱动宠物的音源(自动判定)

protected:
    void showEvent(QShowEvent*) override;
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void dragEnterEvent(QDragEnterEvent*) override;
    void dropEvent(QDropEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    bool eventFilter(QObject* o, QEvent* e) override;   // Dock 点击恢复
    void wheelEvent(QWheelEvent*) override;             // ⌘/滚轮缩放
    void resizeEvent(QResizeEvent*) override;           // 强制方形
    void closeEvent(QCloseEvent*) override;             // 退出前存设置

private slots:
    void onTick();
    void onAnalyze();
    void onWebAudio(const QVector<float>& pcm, int sr, const QString& title);
    void onSystemAudio(const QVector<float>& mono, int sr);   // WASAPI loopback(仅 Windows)

private:
    void stepSim(int dtMs);
    void buildSpectrumGrid();
    void togglePlay();
    void openFile();
    void setManualMode(Tier t);   // 手动切到活动档(固定脸/色,音浪仍随音频)
    void setAutoMode();           // 退回自动(音频驱动情绪)
    void showFocusReport();       // 专注周报对话框(手动活动档时长)
    void showCalibrationDialog(); // 用户 A/B 校准:录欢快/伤感两首 → 个性化 minorShare 分界
    void minimizeToTray();        // 隐藏到托盘
    void restoreFromTray();       // 从托盘恢复(置顶)
    void setSize(int s);          // 中心锚定缩放 + 夹屏 + 持久化
    void applyZoom();             // 合并滚轮 delta 后应用
    void setOpacityLevel(qreal o); // 窗口透明度档(0.40..1.0)+ 持久化
    void setLocked(bool b);        // 锁/解锁位置(光标提示 + 持久化)
    void setEqStyle(PetScene::EqStyle s);    // 音浪地板样式 + 持久化
    void loadSettings();          // 恢复 尺寸/位置/手动模式/透明度/锁定/音浪
    void saveSettings();          // 存    同上
    void applyFeatures(const Features& f);  // 本地/网页音源公共:特征→情绪/流派/音浪 + 调试打印
    void updateIdle(float rms, int dtMs);   // RMS 低累计(按真实 dt)→ idle

    static constexpr int kGW = 48, kGH = 36;

    PetAnimator anim_;
    QTimer timer_;            // 渲染 60fps
    QTimer* analyzer_ = nullptr;   // 音频分析 ~30fps
    QElapsedTimer clock_;
    qint64 lastMs_ = 0;

    BeadGrid faceGrid_{kGW, kGH};
    BeadGrid accentGrid_{kGW, kGH};
    BeadGrid specGrid_{kGW, kGH};
    float spec_[kGW] = {};
    bool haveRealAudio_ = false;

    float simTMs_ = 0.0f;
    float quietMs_ = 0.0f;     // 静默累计(>2s 进 idle)
    float curRms_ = 0.0f;      // 最近一帧 rms(onTick 判 idle 用)
    float lastMinorShare_ = 0.0f;  // 最近一帧 minorShare(校准对话框录制用)
    qint64 lastAudioMs_ = 0;   // 最近音频帧时间(网页超时降级)
    qint64 lastDebugMs_ = 0;   // 调试打印限流
    qint64 lastStatsSaveMs_ = 0;   // 专注统计落盘限流(60s)
    bool webActive_ = false;   // 网页源是否曾连过(决定 idle 是否判网页超时)
    WebStatus webStatus_ = WebStatus::Disconnected;  // 网页连接状态(状态点显示用)
    FocusStats stats_;         // 专注周报(手动活动档时长持久化)

    QPoint dragOffset_;
    bool dragging_ = false;
    int hoverBtn_ = -1;
    bool raised_ = false;     // macOS 首次 show 时提窗口层级(只做一次)
    bool hasHover_ = false;   // 鼠标是否在窗口内(控件淡入用)
    float controlsAlpha_ = 0; // 控件区透明度(0 隐藏 → 1 全显),hover 时渐入

    bool manual_ = false;        // 手动模式(覆盖自动情绪识别)
    Tier manualTier_ = Tier::Calm;
    float beatEnv_ = 0.0f;       // 真实音频低频能量包络(快攻慢衰)→ 踩拍
    bool systemActive_ = false;  // 系统音频(WASAPI)在推数据(接桌面客户端等)
    bool webEnabled_ = true;     // 网页音源开关(设置页;关则忽略扩展数据)
    QString lastTitle_;          // 网页标签页标题(含歌名,profile 采集按歌名分组)

    QSystemTrayIcon* tray_ = nullptr;  // 菜单栏/托盘图标(最小化后恢复用)

    int  side_ = 320;          // 当前边长(方形)
    int  pendingZoom_ = 0;     // 累积滚轮 delta(合并触控板洪流)
    bool zoomQueued_ = false;
    bool inResizeGuard_ = false;

    qreal   opacity_ = 1.0;    // 窗口整体不透明度(透明度调节;0.40..1.0)
    bool    locked_  = false;  // 锁定位置(禁止拖拽,控件/右键仍响应)
    PetScene::EqStyle eqStyle_ = PetScene::EqStyle::Bars;  // 音浪地板样式

    AudioEngine* engine_ = nullptr;
    WebAudioSource* webSource_ = nullptr;
    SystemAudioSource* sysSource_ = nullptr;   // 系统音频(仅 Windows 有 loopback 实现)
    FeatureExtractor fx_;
};
