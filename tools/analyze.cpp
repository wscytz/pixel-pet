// analyze — 无 Qt 头less 音频特征分析器(调参自检工具,不参与发布构建)
//
// 用法:tools/analyze <song.wav> [analysisHz]
//   · wav 由 afconvert 转:PCM 16/24/32bit 或 float32,mono/stereo(自动下混)
//   · 复用真 FeatureExtractor / EmotionMapper,与 GUI 同一条链路,离线秒跑
//   · 每秒一行均值;结尾打印整曲情绪档分布 + mode/margin/调式小调占比
//
// 喂窗方式模拟 GUI:analysisHz=30 帧/秒,每帧把窗口滑 hop=sr/analysisHz,
// compute() 内部取最后 kN 样本做 FFT。

#include "dsp/FeatureExtractor.h"
#include "emotion/Emotion.h"
#include "emotion/EmotionMapper.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// ---- WAV 解析:RIFF/WAVE,跳 LIST/INFO,fmt+data,16/24/32 int 或 float32 ----
static bool readWav(const std::string& path, std::vector<float>& mono, int& sr) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) { std::fprintf(stderr, "cannot open %s\n", path.c_str()); return false; }
    auto rd = [&](void* buf, size_t n) { return std::fread(buf, 1, n, f) == n; };
    char tag[5] = {};
    int channels = 0, bits = 0;
    bool foundData = false;
    long dataSize = 0;
    if (!rd(tag, 4) || std::memcmp(tag, "RIFF", 4) != 0) { std::fprintf(stderr, "not RIFF\n"); return false; }
    unsigned int dummy;
    rd(&dummy, 4);
    if (!rd(tag, 4) || std::memcmp(tag, "WAVE", 4) != 0) { std::fprintf(stderr, "not WAVE\n"); return false; }
    while (rd(tag, 4) && rd(&dummy, 4)) {
        const long chunkStart = std::ftell(f);
        const long chunkSize = dummy;
        if (std::memcmp(tag, "fmt ", 4) == 0) {
            unsigned short fmt, ch;
            unsigned int rate;
            unsigned short bps;
            rd(&fmt, 2); rd(&ch, 2); rd(&rate, 4);
            rd(&dummy, 4); rd(&dummy, 2);   // byteRate, blockAlign
            rd(&bps, 2);
            channels = ch; sr = static_cast<int>(rate); bits = bps;
            if (fmt != 1 && fmt != 3) { std::fprintf(stderr, "unsupported fmt %u\n", fmt); return false; }
        } else if (std::memcmp(tag, "data", 4) == 0) {
            foundData = true; dataSize = chunkSize;
            break;
        }
        std::fseek(f, chunkStart + chunkSize + (chunkSize & 1), SEEK_SET);
    }
    if (!foundData || channels <= 0 || sr <= 0) { std::fprintf(stderr, "bad wav\n"); return false; }

    const int bytesPerSample = bits / 8;
    const long frames = dataSize / (bytesPerSample * channels);
    mono.reserve(static_cast<size_t>(frames));
    std::vector<char> raw(static_cast<size_t>(bytesPerSample * channels));
    for (long i = 0; i < frames; ++i) {
        if (!rd(raw.data(), raw.size())) break;
        double s = 0.0;
        for (int c = 0; c < channels; ++c) {
            const char* p = raw.data() + c * bytesPerSample;
            double v = 0.0;
            if (bits == 16) {
                short sv; std::memcpy(&sv, p, 2); v = sv / 32768.0;
            } else if (bits == 24) {
                const int t = (p[0] & 0xff) | ((p[1] & 0xff) << 8) | ((p[2] & 0xff) << 16);
                v = (t >= 0x800000 ? t - 0x1000000 : t) / 8388608.0;
            } else if (bits == 32) {
                int iv; std::memcpy(&iv, p, 4); v = iv / 2147483648.0;
            } else {  // float32
                float fv; std::memcpy(&fv, p, 4); v = fv;
            }
            s += v;
        }
        mono.push_back(static_cast<float>(s / channels));   // 下混 mono
    }
    std::fclose(f);
    return true;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: analyze <song.wav> [analysisHz]\n");
        return 1;
    }
    std::vector<float> mono;
    int sr = 0;
    if (!readWav(argv[1], mono, sr)) return 1;
    const float analysisHz = argc >= 3 ? static_cast<float>(std::atof(argv[2])) : 30.0f;
    if (analysisHz <= 0.0f || sr <= 0) return 1;
    const int hop = std::max(1, static_cast<int>(sr / analysisHz));
    std::fprintf(stderr, "[analyze] %s sr=%d n=%lld (%.1fs) hop=%d\n",
                 argv[1], sr, static_cast<long long>(mono.size()),
                 static_cast<double>(mono.size()) / sr, hop);

    FeatureExtractor fx;
    // 滚动窗:只保留最近 kN 样本(compute 的 RMS/FFT 都取最后 kN,别让历史 buffer 无限长)
    constexpr int kN = 2048;
    std::vector<float> win;
    win.reserve(kN + hop);

    int tierCount[kTierCount] = {};
    int minorFrames = 0, majorFrames = 0, totalFrames = 0;
    double modeSum = 0, marginSum = 0, kconfSum = 0, valSum = 0, aroSum = 0, bpmSum = 0;
    double msSum = 0;
    int bpmFrames = 0;
    const double framesPerSec = analysisHz;
    int framesThisSec = 0;
    double accMode = 0, accMargin = 0, accKconf = 0, accVal = 0, accAro = 0, accBpm = 0;
    double accMs = 0;
    int accBpmFrames = 0;
    int accTier[kTierCount] = {};
    int sec = 0;
    // minorShare 分桶(校准调式模糊阈值用):<0.30 大调主导 / 0.30-0.45 偏大调 / 0.45-0.55 模糊 / >0.55 小调主导
    int msBucket[4] = {};

    const size_t n = mono.size();
    for (size_t pos = 0; pos < n; pos += static_cast<size_t>(hop)) {
        const size_t end = std::min(n, pos + static_cast<size_t>(hop));
        for (size_t i = pos; i < end; ++i) win.push_back(mono[i]);
        if (static_cast<int>(win.size()) > kN) win.erase(win.begin(), win.begin() + (win.size() - kN));

        Features f;
        fx.compute(win.data(), static_cast<int>(win.size()), sr, analysisHz, f);
        const Emotion e = EmotionMapper::map(f);
        const int ti = static_cast<int>(e.tier);
        const auto& d = fx.lastKeyDiag();

        ++totalFrames;
        if (d.major) ++majorFrames; else ++minorFrames;
        modeSum += f.mode; marginSum += d.margin; kconfSum += f.keyConfidence;
        msSum += f.minorShare;
        valSum += e.valence; aroSum += e.arousal;
        if (f.bpm > 1.0f) { bpmSum += f.bpm; ++bpmFrames; }
        ++tierCount[ti];
        if (f.minorShare < 0.30f) ++msBucket[0];
        else if (f.minorShare < 0.45f) ++msBucket[1];
        else if (f.minorShare < 0.55f) ++msBucket[2];
        else ++msBucket[3];

        accMode += f.mode; accMargin += d.margin; accKconf += f.keyConfidence;
        accMs += f.minorShare;
        accVal += e.valence; accAro += e.arousal;
        if (f.bpm > 1.0f) { accBpm += f.bpm; ++accBpmFrames; }
        ++accTier[ti]; ++framesThisSec;

        if (framesThisSec >= static_cast<int>(framesPerSec)) {
            const double k = framesThisSec;
            int topTier = 0;
            for (int i = 1; i < kTierCount; ++i) if (accTier[i] > accTier[topTier]) topTier = i;
            std::fprintf(stderr, "t=%3d mode=%+5.2f kc=%4.2f mg=%+5.2f ms=%4.2f val=%+5.2f aro=%4.2f bpm=%5.1f top=%s(%d)\n",
                         sec, accMode / k, accKconf / k, accMargin / k, accMs / k,
                         accVal / k, accAro / k,
                         accBpmFrames ? accBpm / accBpmFrames : 0.0,
                         EmotionMapper::def(static_cast<Tier>(topTier)).name, accTier[topTier]);
            sec++;
            framesThisSec = 0;
            accMode = accMargin = accKconf = accVal = accAro = accBpm = accMs = 0;
            accBpmFrames = 0;
            for (int i = 0; i < kTierCount; ++i) accTier[i] = 0;
        }
    }

    const double k = totalFrames;
    std::fprintf(stderr, "\n[sum] frames=%d mode=%+.3f margin=%+.3f kconf=%.3f ms=%.3f val=%+.3f aro=%.3f bpm=%s%.1f\n",
                 totalFrames, modeSum / k, marginSum / k, kconfSum / k, msSum / k,
                 valSum / k, aroSum / k, bpmFrames ? "" : "(none) ", bpmFrames ? bpmSum / bpmFrames : 0.0);
    std::fprintf(stderr, "[sum] key: major=%d minor=%d 小调占比=%.1f%%\n",
                 majorFrames, minorFrames, 100.0 * minorFrames / std::max(1, totalFrames));
    std::fprintf(stderr, "[ms] <0.30大调=%d 0.30-0.45偏大=%d 0.45-0.55模糊=%d >0.55小调=%d\n",
                 msBucket[0], msBucket[1], msBucket[2], msBucket[3]);
    std::fprintf(stderr, "[dist] ");
    for (int i = 0; i < kTierCount; ++i) {
        if (tierCount[i]) std::fprintf(stderr, "%s=%d ", EmotionMapper::def(static_cast<Tier>(i)).name, tierCount[i]);
    }
    std::fprintf(stderr, "\n");
    return 0;
}
