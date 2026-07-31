#pragma once

#include <QObject>
#include <QVector>
#include <QString>
#include <QList>
#include <QElapsedTimer>
#include <cstdint>

class QWebSocketServer;
class QWebSocket;
class QTimer;

// 网页音源连接状态(状态点用):
//   Disconnected 没连/连了没推数据   Connected 连上且在推音频   Error ws 监听失败
enum class WebStatus { Disconnected, Connected, Error };

// 网页音源:本地 WebSocket server(127.0.0.1:17632),收浏览器插件推来的
// {spectrum:[...], rms, title} JSON,解析后发 audioFrame;并维护连接状态发 statusChanged。
class WebAudioSource : public QObject {
    Q_OBJECT
public:
    explicit WebAudioSource(QObject* parent = nullptr);

signals:
    void audioFrame(const QVector<float>& pcm, int sampleRate, const QString& title);
    void statusChanged(WebStatus s);

private:
    void checkStatus();   // 周期按 监听/连接/最近帧 刷新状态

    QWebSocketServer* server_ = nullptr;
    QList<QWebSocket*> clients_;
    QWebSocket* activeClient_ = nullptr;   // 多源时只认最近推数据的连接,避免频谱互相覆盖
    QTimer* monitor_ = nullptr;            // 周期检查状态(超时降级 Disconnected)
    QElapsedTimer clock_;
    qint64 lastFrameMs_ = -100000;         // 最近收到有效 frame 的时刻(相对 clock_)
    bool hasClient_ = false;
    bool listenOk_ = false;
    WebStatus status_ = WebStatus::Disconnected;
};
