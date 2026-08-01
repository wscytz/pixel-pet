#include "audio/WebAudioSource.h"

#include <QWebSocketServer>
#include <QWebSocket>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <algorithm>
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
            // 只认浏览器扩展来源:非空 origin 且非 chrome-extension → 拒绝(本地恶意网页不能驱动/干扰桌宠);空 origin(本地调试)放行
            const QString origin = c->origin();
            if (!origin.isEmpty() && !origin.startsWith(QStringLiteral("chrome-extension://"))) {
                c->close(QWebSocketProtocol::CloseCodePolicyViolated, QStringLiteral("origin rejected"));
                c->deleteLater();
                return;
            }
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
                    if (arr.size() <= 0 || arr.size() > 8192) return;   // 先查大小再分配(防超大数组)
                    QVector<float> pcm;
                    pcm.reserve(arr.size());
                    for (const auto& v : arr)
                        pcm.append(static_cast<float>(v.toDouble()));
                    const int sr = std::clamp(o.value("sr").toInt(44100), 8000, 192000);   // 防除零/极端采样率
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
                c->deleteLater();   // server 不拥有 socket,caller 不删即泄漏(切页/重载累积)
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
