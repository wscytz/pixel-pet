#include "audio/AudioEngine.h"

#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QAudioFormat>
#include <QAudioSink>
#include <QBuffer>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <memory>

AudioEngine::AudioEngine(QObject* parent) : QObject(parent) {}

void AudioEngine::load(const QString& path) {
    if (activeDec_) { activeDec_->stop(); activeDec_->deleteLater(); activeDec_ = nullptr; }   // 取消在途解码(load 重入)
    auto* dec = new QAudioDecoder(this);
    activeDec_ = dec;
    auto acc = std::make_shared<std::vector<float>>();

    connect(dec, &QAudioDecoder::bufferReady, this, [this, dec, acc]() {
        const QAudioBuffer ab = dec->read();
        if (!ab.isValid()) return;
        const QAudioFormat& fmt = ab.format();
        sr_ = fmt.sampleRate();
        const int ch = fmt.channelCount();
        const int frames = ab.frameCount();
        const auto sf = fmt.sampleFormat();

        if (sf == QAudioFormat::Float) {
            const float* d = ab.constData<float>();
            for (int i = 0; i < frames; ++i) {
                float s = 0.0f;
                for (int c = 0; c < ch; ++c) s += d[i * ch + c];
                acc->push_back(s / ch);
            }
        } else if (sf == QAudioFormat::Int16) {
            const qint16* d = ab.constData<qint16>();
            for (int i = 0; i < frames; ++i) {
                float s = 0.0f;
                for (int c = 0; c < ch; ++c) s += d[i * ch + c] / 32768.0f;
                acc->push_back(s / ch);
            }
        } else if (sf == QAudioFormat::Int32) {
            const qint32* d = ab.constData<qint32>();
            for (int i = 0; i < frames; ++i) {
                float s = 0.0f;
                for (int c = 0; c < ch; ++c) s += d[i * ch + c] / 2147483648.0f;
                acc->push_back(s / ch);
            }
        } else if (sf == QAudioFormat::UInt8) {
            const quint8* d = ab.constData<quint8>();
            for (int i = 0; i < frames; ++i) {
                float s = 0.0f;
                for (int c = 0; c < ch; ++c) s += (static_cast<int>(d[i * ch + c]) - 128) / 128.0f;
                acc->push_back(s / ch);
            }
        }
    });
    connect(dec, &QAudioDecoder::finished, this, [this, dec, acc]() {
        if (activeDec_ == dec) activeDec_ = nullptr;
        mono_ = std::move(*acc);
        buildRaw();
        dec->deleteLater();
        // 换新源:清掉旧播放状态(暂停/继续的 resume 只对同一流有效,换曲必须从头)
        if (sink_) { sink_->stop(); delete sink_; sink_ = nullptr; }
        if (buf_) { buf_->close(); delete buf_; buf_ = nullptr; }
        if (mono_.empty()) emit failed("解码为空(格式不支持或读取出错)");
        else emit loaded();
    });
    connect(dec, QOverload<QAudioDecoder::Error>::of(&QAudioDecoder::error), this, [this, dec](QAudioDecoder::Error) {
        if (activeDec_ == dec) activeDec_ = nullptr;
        const QString msg = dec->errorString();
        dec->deleteLater();
        emit failed(msg.isEmpty() ? QStringLiteral("解码失败") : msg);
    });

    dec->setSource(QUrl::fromLocalFile(path));
    dec->start();
}

void AudioEngine::buildRaw() {
    raw_.clear();
    raw_.reserve(mono_.size() * 4);
    for (float s : mono_) {
        s = std::clamp(s, -1.0f, 1.0f);
        const char* p = reinterpret_cast<const char*>(&s);
        raw_.append(p, 4);
    }
}

void AudioEngine::play() {
    if (mono_.empty()) return;
    if (sink_ && sink_->state() == QAudio::SuspendedState) {   // 暂停→继续:resume 当前流,别从头重播
        sink_->resume();
        return;
    }
    QAudioFormat fmt;
    fmt.setSampleRate(sr_);
    fmt.setChannelCount(1);
    fmt.setSampleFormat(QAudioFormat::Float);
    delete sink_; sink_ = nullptr;
    if (buf_) { buf_->close(); delete buf_; buf_ = nullptr; }
    buf_ = new QBuffer(&raw_, this);
    buf_->open(QIODevice::ReadOnly);
    sink_ = new QAudioSink(fmt, this);
    connect(sink_, &QAudioSink::stateChanged, this, [this](QAudio::State st) {
        if (st == QAudio::IdleState) emit ended();
    });
    sink_->start(buf_);
}

void AudioEngine::pause() { if (sink_) sink_->suspend(); }
void AudioEngine::stop()  { if (sink_) sink_->stop(); }
bool AudioEngine::isPlaying() const { return sink_ && sink_->state() == QAudio::ActiveState; }

int AudioEngine::readMonoWindow(float* out, int frames) const {
    if (!buf_ || mono_.empty()) return 0;
    const qint64 pos = buf_->pos() / 4;   // float = 4 字节
    const int cur = static_cast<int>(pos);
    const int n = std::min(frames, cur);  // 取已播放部分
    if (n <= 0) return 0;
    const int start = cur - n;
    for (int i = 0; i < n; ++i) out[i] = mono_[start + i];
    return n;
}
