#include "audio/WebAudioSource.h"

#include <QWebSocketServer>
#include <QWebSocket>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <cmath>

WebAudioSource::WebAudioSource(QObject* parent) : QObject(parent) {
    clock_.start();
    server_ = new QWebSocketServer(QStringLiteral("PixelPet"),
                                   QWebSocketServer::NonSecureMode, this);
    listenOk_ = server_->listen(QHostAddress::LocalHost, 17632);

    // 周期检查:有连接但 >2s 没推数据 → 降级 Disconnected;监听失败 → Error
    monitor_ = new QTimer(this);
    monitor_->setInterval(500);
    connect(monitor_, &QTimer::timeout, this, &WebAudioSource::checkStatus);
    monitor_->start();

    if (server_) {
        connect(server_, &QWebSocketServer::newConnection, this, [this]() {
            QWebSocket* c = server_->nextPendingConnection();
            clients_ << c;
            hasClient_ = true;
            connect(c, &QWebSocket::textMessageReceived, this,
                [this, c](const QString& msg) {
                    // 多源(多标签/多插件)只认最近活跃连接,避免频谱互相覆盖
                    if (activeClient_ && c != activeClient_) return;
                    activeClient_ = c;
                    const QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
                    const QJsonObject o = doc.object();
                    const QJsonArray arr = o.value("pcm").toArray();
                    QVector<float> pcm;
                    pcm.reserve(arr.size());
                    for (const auto& v : arr)
                        pcm.append(static_cast<float>(v.toDouble()));
                    const int sr = o.value("sr").toInt(44100);
                    const QString title = o.value("title").toString();
                    if (pcm.isEmpty() || pcm.size() > 8192) return;   // 坏包/过大校验
                    emit audioFrame(pcm, sr, title);
                    // 收到有效帧 → 立即标绿(不等 500ms 轮询)
                    lastFrameMs_ = clock_.elapsed();
                    if (status_ != WebStatus::Connected) {
                        status_ = WebStatus::Connected;
                        emit statusChanged(WebStatus::Connected);
                    }
                });
            connect(c, &QWebSocket::disconnected, this, [this, c]() {
                clients_.removeAll(c);
                if (activeClient_ == c) activeClient_ = nullptr;
                hasClient_ = !clients_.isEmpty();
            });
        });
    }

    checkStatus();   // 初始状态(listen 失败 → Error)
}

void WebAudioSource::checkStatus() {
    WebStatus want;
    if (!listenOk_) {
        want = WebStatus::Error;
    } else if (hasClient_ && (clock_.elapsed() - lastFrameMs_ < 2000)) {
        want = WebStatus::Connected;
    } else {
        want = WebStatus::Disconnected;
    }
    if (want != status_) {
        status_ = want;
        emit statusChanged(want);
    }
}
