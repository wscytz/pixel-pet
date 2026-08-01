#pragma once

#include <QObject>
#include <QByteArray>
#include <vector>

class QAudioSink;
class QBuffer;
class QAudioDecoder;

// 音频引擎:解码本地文件 → mono PCM 缓冲 → QAudioSink 播放;
// 分析侧可读取"当前播放位置"的 PCM 窗口喂给 FFT(播放与频谱同源,无需系统 loopback)。
class AudioEngine : public QObject {
    Q_OBJECT
public:
    explicit AudioEngine(QObject* parent = nullptr);

    void load(const QString& path);   // 异步解码,完成发 loaded()
    void play();
    void pause();
    void stop();
    bool isPlaying() const;
    int  sampleRate() const { return sr_; }

    // 取当前播放位置往前 frames 样本(mono)到 out,返回实际样本数
    int readMonoWindow(float* out, int frames) const;

signals:
    void loaded();
    void failed(const QString& msg);
    void ended();

private:
    void buildRaw();

    std::vector<float> mono_;   // mono float PCM
    int  sr_ = 44100;
    QByteArray raw_;            // mono float 字节流(给 QBuffer)
    QAudioSink* sink_ = nullptr;
    QBuffer*    buf_  = nullptr;
    QAudioDecoder* activeDec_ = nullptr;   // 在途解码(load 重入时取消旧的,防并发解码互相覆盖)
};
