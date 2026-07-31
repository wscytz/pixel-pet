#pragma once

#include <QObject>
#include <QVector>

// 系统音频监听(loopback):捕获系统默认输出设备的混音,下混成 mono PCM 喂 FeatureExtractor。
// 用途:接桌面音乐客户端等「不在浏览器里」的音源(网页版走 browser-extension,不走这里)。
// 跨端:仅 Windows 有 WASAPI loopback 实现;其余平台 start() 返回 false(mac 靠网页扩展兜底)。
// 信号 pcmFrame 在 WASAPI 捕获线程 emit,经 Qt 队列跨线程到主线程(参数 QVector 值语义,安全)。
class SystemAudioSource : public QObject {
    Q_OBJECT
public:
#ifdef Q_OS_WIN
    explicit SystemAudioSource(QObject* parent = nullptr);
    ~SystemAudioSource();
    bool start();    // 启动 WASAPI loopback(默认渲染端点 eRender/eConsole);成功返 true
    void stop();
#else
    explicit SystemAudioSource(QObject* parent = nullptr) : QObject(parent) {}
    ~SystemAudioSource() = default;
    bool start() { return false; }   // 非 Windows:不支持系统监听(菜单项隐藏)
    void stop() {}
#endif
    bool isActive() const { return active_; }

signals:
    void pcmFrame(const QVector<float>& mono, int sampleRate);   // 单声道窗口样本(-1..1)
    void activeChanged(bool on);                                 // 捕获启停

private:
    bool active_ = false;
#ifdef Q_OS_WIN
    struct Impl;
    Impl* impl_ = nullptr;
    void loop();    // 捕获线程主循环
#endif
};
