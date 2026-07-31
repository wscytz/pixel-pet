#include "dsp/FeatureExtractor.h"

#include "dsp/FFT.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
constexpr float kPi = 3.14159265358979f;
constexpr float kEps = 1e-7f;

// 场景判别:静音 / 噪声(游戏音效)/ 人声语音 / 音乐。
//   噪声:谱平坦度高(宽带,无明显谐波结构)。
//   人声:几乎无低频能量(没贝斯/底鼓)+ 中频集中(共振峰 200Hz~4kHz)。
//   音乐:有低频脉动(贝斯/鼓)或明显谐波结构。
// 阈值是起点,靠 stderr 实测打印手调。
SoundClass classifySound(float rms, float flatness, float lowRatio, float centroid) {
    if (rms < 0.012f) return SoundClass::Silence;
    if (flatness > 0.48f) return SoundClass::Noise;   // EDM 合成段 flatness 偏高(0.2~0.5),提到 0.48 让多数走 Music
    if (lowRatio < 0.10f && centroid > 0.12f && centroid < 0.60f) return SoundClass::Voice;
    return SoundClass::Music;
}

// 场景平滑:新类需连续 3 帧才采纳,抗 EDM 合成段 flatness 偶发越界(0.5+)导致的 Music↔Noise 单帧抖动
SoundClass smoothSound(SoundClass prev, SoundClass raw, int& dissent) {
    if (raw == prev) { dissent = 0; return prev; }
    if (++dissent >= 3) { dissent = 0; return raw; }
    return prev;
}

// Krumhansl-Kessler 调式 profile(C-based,12 半音)。chroma 旋转对齐 tonic → Pearson 最大者定调式。
// 大调→正 mode、小调→负 mode,|mode|=置信度。这是 valence 的"基因":大调偏正、小调偏负。
// (Temperley profile 实测更准,但 K-K 是公开常数且广泛引用,先用;要换只改这两行。)
constexpr float KMAJOR[12] = {6.35f,2.23f,3.48f,2.33f,4.38f,4.09f,2.52f,5.19f,2.39f,3.66f,2.29f,2.88f};
constexpr float KMINOR[12] = {6.33f,2.68f,3.52f,5.38f,2.60f,3.53f,2.54f,4.75f,3.98f,2.69f,1.75f,2.66f};

float pearson12(const float* a, const float* b) {
    double ma = 0, mb = 0;
    for (int i = 0; i < 12; ++i) { ma += a[i]; mb += b[i]; }
    ma /= 12; mb /= 12;
    double sxy = 0, sxx = 0, syy = 0;
    for (int i = 0; i < 12; ++i) { const double da = a[i]-ma, db = b[i]-mb; sxy += da*db; sxx += da*da; syy += db*db; }
    const double d = std::sqrt(sxx * syy);
    return d > 0 ? static_cast<float>(sxy / d) : 0.0f;
}

// chroma[12] → mode(-1..+1,小调负/大调正)+ conf(0..1,Pearson 最大相关)
// 顺带填 diag:大小调各自最佳 Pearson + 胜者 tonic + 胜负差距 margin(离线调参用)。
void estimateKey(const float* chroma, float& mode, float& conf,
                 FeatureExtractor::KeyDiag& diag) {
    float best = -2.0f; bool major = true; int bestTonic = 0;
    float bestMajor = -2.0f, bestMinor = -2.0f;
    for (int rot = 0; rot < 12; ++rot) {           // 旋转 chroma 让 tonic 对齐 profile[0]
        float rc[12];
        for (int i = 0; i < 12; ++i) rc[i] = chroma[(i + rot) % 12];
        const float rm = pearson12(rc, KMAJOR);
        const float rn = pearson12(rc, KMINOR);
        if (rm > bestMajor) bestMajor = rm;
        if (rn > bestMinor) bestMinor = rn;
        if (rm > best) { best = rm; major = true; bestTonic = rot; }
        if (rn > best) { best = rn; major = false; bestTonic = rot; }
    }
    conf = best < 0.0f ? 0.0f : (best > 1.0f ? 1.0f : best);
    mode = major ? conf : -conf;
    diag.majorBest = bestMajor;
    diag.minorBest = bestMinor;
    diag.tonic = bestTonic;
    diag.major = major;
    diag.margin = bestMajor - bestMinor;
}
}  // namespace

void FeatureExtractor::compute(const float* mono, int n, int sampleRate,
                               float analysisHz, Features& out) {
    static thread_local std::vector<std::complex<float>> buf(kN);
    if (static_cast<int>(buf.size()) < kN) buf.resize(kN);
    if (static_cast<int>(prevMag_.size()) < kN / 2) prevMag_.assign(kN / 2, 0.0f);

    // 取最后 kN 个样本,不足补零,Hann 窗
    const int start = std::max(0, n - kN);
    for (int i = 0; i < kN; ++i) {
        const int idx = start + i;
        const float s = (idx < n) ? mono[idx] : 0.0f;
        const float w = 0.5f * (1.0f - std::cos(2.0f * kPi * i / (kN - 1)));   // Hann 窗(主瓣窄、分辨率好;Blackman 主瓣太宽致峰糊)
        buf[i] = std::complex<float>(s * w, 0.0f);
    }
    FFT::forward(buf.data(), kN);

    static thread_local std::vector<float> mag(kN / 2);
    if (static_cast<int>(mag.size()) < kN / 2) mag.resize(kN / 2);
    for (int i = 0; i < kN / 2; ++i) mag[i] = std::abs(buf[i]);

    // 48 频段 log
    const float fmin = 30.0f;
    const float fmax = sampleRate / 2.0f;
    for (int b = 0; b < 48; ++b) {
        const float f0 = fmin * std::pow(fmax / fmin, b / 48.0f);
        const float f1 = fmin * std::pow(fmax / fmin, (b + 1) / 48.0f);
        int i0 = static_cast<int>(f0 / sampleRate * kN);
        int i1 = static_cast<int>(f1 / sampleRate * kN);
        i0 = std::clamp(i0, 0, kN / 2 - 1);
        i1 = std::clamp(i1, 0, kN / 2);
        float sum = 0.0f;
        int cnt = 0;
        for (int i = i0; i < i1; ++i) { sum += mag[i]; ++cnt; }
        float v = cnt > 0 ? sum / cnt : 0.0f;
        v = std::log1p(v * 12.0f) / std::log1p(12.0f);
        v = std::clamp(v, 0.0f, 1.0f);
        out.spectrum[b] = prevSpec_[b] * 0.55f + v * 0.45f;
        prevSpec_[b] = out.spectrum[b];
    }

    // RMS:与 FFT 一样取「最后 kN 样本」窗(若传入超过 kN 的历史 buffer 也不会取到开头)
    const int rmsStart = std::max(0, n - kN);
    double sq = 0.0;
    for (int i = rmsStart; i < n; ++i) sq += static_cast<double>(mono[i]) * mono[i];
    out.rms = std::clamp(static_cast<float>(std::sqrt(sq / std::max(1, n - rmsStart))), 0.0f, 1.0f);

    // 频谱重心
    double sumf = 0.0, summ = 0.0;
    for (int i = 1; i < kN / 2; ++i) {
        const float f = static_cast<float>(i) * sampleRate / kN;
        sumf += static_cast<double>(f) * mag[i];
        summ += mag[i];
    }
    const float cf = summ > 0 ? static_cast<float>(sumf / summ) : 0.0f;
    out.centroid = std::clamp(cf / (sampleRate / 2.0f), 0.0f, 1.0f);

    // 低频分界(<=250Hz):beat 在这里 → 既是 BPM onset 区,也是音乐 vs 人声判据。
    // 节奏型另开两带:kick(<=120Hz 底鼓)/ snare(1-4kHz 军鼓/掌声)。
    const int M = kN / 2;
    const int lowIdx = std::max(2, static_cast<int>(250.0f / sampleRate * kN));
    const int kickIdx = std::max(2, static_cast<int>(120.0f / sampleRate * kN));
    const int bodyLo = std::max(1, static_cast<int>(150.0f / sampleRate * kN));
    const int bodyHi = std::min(M, static_cast<int>(300.0f / sampleRate * kN));
    const int snareLo = std::max(1, static_cast<int>(1000.0f / sampleRate * kN));
    const int snareHi = std::min(M, static_cast<int>(4000.0f / sampleRate * kN));
    const int subIdx[5] = {snareLo,
                           snareLo + (snareHi - snareLo) / 4,
                           snareLo + 2 * (snareHi - snareLo) / 4,
                           snareLo + 3 * (snareHi - snareLo) / 4,
                           snareHi};

    // flatness + lowRatio + 低频 onset(BPM 用)。遍历一次算齐。
    double logSum = 0.0, magSum = 0.0, lowE = 0.0, totalE = 0.0;
    float lowFlux = 0.0f, kickFlux = 0.0f, bodyFlux = 0.0f, snareFlux = 0.0f;
    float snareB[4] = {};
    for (int i = 1; i < M; ++i) {
        const float m = mag[i] + kEps;
        logSum += std::log(m);
        magSum += m;
        const double e = static_cast<double>(mag[i]) * mag[i];
        totalE += e;
        if (i < lowIdx) lowE += e;
        const float d = mag[i] - prevMag_[i];
        if (d > 0.0f && i < lowIdx) lowFlux += d;
        if (d > 0.0f && i < kickIdx) kickFlux += d;
        if (d > 0.0f && i >= bodyLo && i < bodyHi) bodyFlux += d;
        if (i >= snareLo && i < snareHi) {
            if (d > 0.0f) snareFlux += d;
            for (int k = 0; k < 4; ++k)
                if (d > 0.0f && i >= subIdx[k] && i < subIdx[k + 1]) snareB[k] += d;
        }
    }
    std::copy(mag.begin(), mag.begin() + M, prevMag_.begin());
    out.kickFlux = kickFlux;
    out.bodyFlux = bodyFlux;
    out.snareFlux = snareFlux;
    for (int k = 0; k < 4; ++k) out.snareBands[k] = snareB[k];

    const int cnt = M - 1;
    out.flatness = (magSum > kEps && cnt > 0)
        ? std::clamp(static_cast<float>(std::exp(logSum / cnt) / (magSum / cnt)), 0.0f, 1.0f)
        : 0.0f;
    out.lowRatio = (totalE > 0.0) ? std::clamp(static_cast<float>(lowE / totalE), 0.0f, 1.0f) : 0.0f;
    out.sound = smoothSound(prevSound_, classifySound(out.rms, out.flatness, out.lowRatio, out.centroid), soundDissent_);
    prevSound_ = out.sound;

    // chroma(12 半音,100-5000Hz 能量累积到 pitch class)→ 调式(mode/conf),valence 的"基因"
    {
        float chroma[12] = {};
        for (int i = 1; i < M; ++i) {
            const float f = static_cast<float>(i) * sampleRate / kN;
            if (f < 100.0f || f > 5000.0f) continue;   // 100Hz:150 会弱化 dubstep(去 sub-bass 致 mode -0.6→-0.3),保持 100
            int p = static_cast<int>(std::lround(69.0f + 12.0f * std::log2(f / 440.0f)));
            p = ((p % 12) + 12) % 12;
            chroma[p] += mag[i];
        }
        for (int c = 0; c < 12; ++c) prevChroma_[c] = prevChroma_[c] * 0.85f + chroma[c] * 0.15f;
        estimateKey(prevChroma_.data(), out.mode, out.keyConfidence, keyDiag_);
        out.keyMargin = keyDiag_.margin;
        minorShare_ = minorShare_ * 0.98f + (keyDiag_.major ? 0.0f : 1.0f) * 0.02f;
        out.minorShare = minorShare_;
        sectionDet_.feed(prevChroma_.data(), out.rms, analysisHz);   // 结构段:平滑后 chroma + rms
        out.section = sectionDet_.section();
    }

    onsetHistory_.push_back(lowFlux);   // BPM 用低频 onset(抗高频干扰)
    updateBpm(analysisHz, out);
    rhythmDet_.feed(kickFlux, bodyFlux, out.snareBands.data(), out.bpm, out.bpmConfidence, analysisHz);   // 节奏型
    out.rhythmBackbeat = rhythmDet_.backbeat();
    out.rhythmKickDensity = rhythmDet_.kickDensity();
    out.rhythmSyncop = rhythmDet_.syncop();
}

void FeatureExtractor::computeFromBins(const uint8_t* bins, int n, float rms, Features& out) {
    if (static_cast<int>(prevBinsMag_.size()) < n) prevBinsMag_.assign(n, 0.0f);
    static thread_local std::vector<float> mag;
    mag.assign(n, 0.0f);
    for (int i = 0; i < n; ++i) mag[i] = bins[i] / 255.0f;

    // 48 段 log(bin index)
    for (int b = 0; b < 48; ++b) {
        int i0 = static_cast<int>(std::pow(static_cast<float>(n), b / 48.0f));
        int i1 = static_cast<int>(std::pow(static_cast<float>(n), (b + 1) / 48.0f));
        i0 = std::clamp(i0, 0, n - 1);
        i1 = std::clamp(i1, 0, n);
        float sum = 0.0f;
        int cnt = 0;
        for (int i = i0; i < i1; ++i) { sum += mag[i]; ++cnt; }
        float v = cnt > 0 ? sum / cnt : 0.0f;
        v = std::log1p(v * 12.0f) / std::log1p(12.0f);
        v = std::clamp(v, 0.0f, 1.0f);
        out.spectrum[b] = prevSpec_[b] * 0.55f + v * 0.45f;
        prevSpec_[b] = out.spectrum[b];
    }

    out.rms = std::clamp(rms, 0.0f, 1.0f);

    double sf = 0.0, sm = 0.0;
    for (int i = 0; i < n; ++i) { sf += static_cast<double>(i) * mag[i]; sm += mag[i]; }
    out.centroid = sm > 0 ? std::clamp(static_cast<float>(sf / sm / n), 0.0f, 1.0f) : 0.0f;

    // 低频分界:网页 bins 线性铺到 nyquist,低频分辨率有限,取前 1/16 当低频区。
    const int lowIdx = std::max(2, n / 16);
    const int kickIdx = std::max(2, static_cast<int>(120.0f / 24000.0f * n));
    const int bodyLo = std::max(1, static_cast<int>(150.0f / 24000.0f * n));
    const int bodyHi = std::min(n, static_cast<int>(300.0f / 24000.0f * n));
    const int snareLo = std::max(1, static_cast<int>(1000.0f / 24000.0f * n));
    const int snareHi = std::min(n, static_cast<int>(4000.0f / 24000.0f * n));
    const int subIdx[5] = {snareLo,
                           snareLo + (snareHi - snareLo) / 4,
                           snareLo + 2 * (snareHi - snareLo) / 4,
                           snareLo + 3 * (snareHi - snareLo) / 4,
                           snareHi};
    double logSum = 0.0, magSum = 0.0, lowE = 0.0, totalE = 0.0;
    float lowFlux = 0.0f, kickFlux = 0.0f, bodyFlux = 0.0f, snareFlux = 0.0f;
    float snareB[4] = {};
    for (int i = 0; i < n; ++i) {
        const float m = mag[i] + kEps;
        logSum += std::log(m);
        magSum += m;
        const double e = static_cast<double>(mag[i]) * mag[i];
        totalE += e;
        if (i < lowIdx) lowE += e;
        const float d = mag[i] - prevBinsMag_[i];
        if (d > 0.0f && i < lowIdx) lowFlux += d;
        if (d > 0.0f && i < kickIdx) kickFlux += d;
        if (d > 0.0f && i >= bodyLo && i < bodyHi) bodyFlux += d;
        if (i >= snareLo && i < snareHi) {
            if (d > 0.0f) snareFlux += d;
            for (int k = 0; k < 4; ++k)
                if (d > 0.0f && i >= subIdx[k] && i < subIdx[k + 1]) snareB[k] += d;
        }
    }
    std::copy(mag.begin(), mag.end(), prevBinsMag_.begin());
    out.kickFlux = kickFlux;
    out.bodyFlux = bodyFlux;
    out.snareFlux = snareFlux;
    for (int k = 0; k < 4; ++k) out.snareBands[k] = snareB[k];

    out.flatness = (magSum > kEps && n > 0)
        ? std::clamp(static_cast<float>(std::exp(logSum / n) / (magSum / n)), 0.0f, 1.0f)
        : 0.0f;
    out.lowRatio = (totalE > 0.0) ? std::clamp(static_cast<float>(lowE / totalE), 0.0f, 1.0f) : 0.0f;
    out.sound = smoothSound(prevSound_, classifySound(out.rms, out.flatness, out.lowRatio, out.centroid), soundDissent_);
    prevSound_ = out.sound;

    // chroma(网页 bins 线性映射 nyquist,假设 sr=44100;低频 bin 少分辨率有限,粗估)→ 调式
    {
        float chroma[12] = {};
        const float nyq = 24000.0f;   // 先假设网页 AudioContext sr=48000(macOS 浏览器常见),验证频率映射是否偏差
        for (int i = 1; i < n; ++i) {
            const float f = static_cast<float>(i) * nyq / n;
            if (f < 100.0f || f > 5000.0f) continue;   // 100Hz:150 会弱化 dubstep(去 sub-bass 致 mode -0.6→-0.3),保持 100
            int p = static_cast<int>(std::lround(69.0f + 12.0f * std::log2(f / 440.0f)));
            p = ((p % 12) + 12) % 12;
            // 网页 bins 是 getByteFrequencyData(dB 压缩到 0-255,非线性幅度)→ 转回 linear 才与 PCM 一致
            chroma[p] += std::pow(10.0f, (-100.0f + (bins[i] / 255.0f) * 70.0f) / 20.0f);
        }
        for (int c = 0; c < 12; ++c) prevChroma_[c] = prevChroma_[c] * 0.85f + chroma[c] * 0.15f;
        estimateKey(prevChroma_.data(), out.mode, out.keyConfidence, keyDiag_);
        out.keyMargin = keyDiag_.margin;
        minorShare_ = minorShare_ * 0.98f + (keyDiag_.major ? 0.0f : 1.0f) * 0.02f;
        out.minorShare = minorShare_;
        sectionDet_.feed(prevChroma_.data(), out.rms, 30.0f);        // 网页 ~30Hz
        out.section = sectionDet_.section();
    }

    onsetHistory_.push_back(lowFlux);
    updateBpm(30.0f, out);  // 插件推送 ~30Hz
    rhythmDet_.feed(kickFlux, bodyFlux, out.snareBands.data(), out.bpm, out.bpmConfidence, 30.0f);   // 节奏型
    out.rhythmBackbeat = rhythmDet_.backbeat();
    out.rhythmKickDensity = rhythmDet_.kickDensity();
    out.rhythmSyncop = rhythmDet_.syncop();
}

void FeatureExtractor::updateBpm(float analysisHz, Features& out) {
    if (onsetHistory_.size() > 240) onsetHistory_.erase(onsetHistory_.begin());
    const int N = static_cast<int>(onsetHistory_.size());
    if (N > 60 && analysisHz > 0.0f) {
        const int minLag = std::max(2, static_cast<int>(analysisHz * 60.0f / 180.0f));
        const int maxLag = std::min(N - 1, static_cast<int>(analysisHz * 60.0f / 50.0f));
        // 归一化自相关:值域 0..1,峰值即拍子规律性(=置信度),抗 onset 绝对能量漂移
        float best = -1.0f;
        int bestLag = minLag;
        for (int lag = minLag; lag <= maxLag; ++lag) {
            double sxy = 0.0, sxx = 0.0, syy = 0.0;
            for (int t = lag; t < N; ++t) {
                const double x = onsetHistory_[t], y = onsetHistory_[t - lag];
                sxy += x * y; sxx += x * x; syy += y * y;
            }
            const float acf = static_cast<float>(sxy / std::sqrt(sxx * syy + 1e-12));
            if (acf > best) { best = acf; bestLag = lag; }
        }
        const float bpm = 60.0f * analysisHz / static_cast<float>(bestLag);
        bpmHistory_.push_back(bpm);
        if (bpmHistory_.size() > 7) bpmHistory_.erase(bpmHistory_.begin());
        std::vector<float> s = bpmHistory_;
        std::sort(s.begin(), s.end());
        const float med = s[s.size() / 2];   // 中值滤波去野值
        prevBpm_ = (prevBpm_ < 1.0f) ? med : (prevBpm_ * 0.85f + med * 0.15f);
        out.bpm = prevBpm_;
        out.bpmConfidence = std::clamp(best, 0.0f, 1.0f);
    } else {
        out.bpm = 0.0f;
        out.bpmConfidence = 0.0f;
    }
}
