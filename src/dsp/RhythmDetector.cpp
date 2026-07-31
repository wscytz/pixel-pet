#include "dsp/RhythmDetector.h"

#include <algorithm>

void RhythmDetector::feed(float kick, float snare, float bpm, float bpmConf, float analysisHz) {
    if (analysisHz <= 0.0f) analysisHz = 30.0f;

    // 包络:快攻慢衰(压帧噪,凸显 onsets;kick 瞬发、持续短)
    const float atk = 0.5f, dec = 0.90f;
    if (kick  > kickEnv_)  kickEnv_  += (kick  - kickEnv_)  * atk; else kickEnv_  *= dec;
    if (snare > snareEnv_) snareEnv_ += (snare - snareEnv_) * atk; else snareEnv_ *= dec;

    // 拍相位累计(bpm 可信才累积,否则直方图只衰减 → 特征归零)
    if (bpm > 1.0f && bpmConf > 0.25f) {
        beatPeriod_ = analysisHz * 60.0f / bpm;
        const float barPeriod = 4.0f * beatPeriod_;
        phaseFrames_ += 1.0f;
        if (phaseFrames_ >= barPeriod) phaseFrames_ -= barPeriod;
        int b = static_cast<int>(phaseFrames_ / barPeriod * kBin);
        if (b >= kBin) b = kBin - 1;
        kickHist_[b] += kickEnv_;
        snareHist_[b] += snareEnv_;
    }

    // 历史衰减(≈最近 ~16s,反映最近几小节)
    for (int i = 0; i < kBin; ++i) { kickHist_[i] *= 0.998f; snareHist_[i] *= 0.998f; }

    // 自旋对齐:最强 kick 相桶 → bin0(downbeat)
    int pivot = 0;
    for (int i = 1; i < kBin; ++i) if (kickHist_[i] > kickHist_[pivot]) pivot = i;
    float kickR[kBin], snareR[kBin];
    for (int i = 0; i < kBin; ++i) {
        kickR[i]  = kickHist_[(i + pivot) % kBin];
        snareR[i] = snareHist_[(i + pivot) % kBin];
    }

    // 特征:拍起点 {0,4,8,12}、offbeat {2,6,10,14}、反拍 snare {4,12}
    float kTotal = 0.0f, sTotal = 0.0f, kOff = 0.0f, kMax = 0.0f;
    for (int i = 0; i < kBin; ++i) {
        kTotal += kickR[i];
        sTotal += snareR[i];
        if (kickR[i] > kMax) kMax = kickR[i];
    }
    for (int i = 2; i < kBin; i += 4) kOff  += kickR[i];

    const float kd = std::max(1e-6f, kTotal);
    syncop_ += (kOff / kd - syncop_) * 0.05f;
    // 反拍:snare 落 2/4 拍 {4,12}。但 kick 1&3 的 rock 会被 BPM 读成半速(kick 间距),
    // snare 落到 offbeat {2,6,10,14} —— 用「拍位或 offbeat 取更大」兜底(两种 tempo 解读都算反拍)。
    float sBeat = 0.0f, sOff = 0.0f;
    for (int i = 4; i < kBin; i += 8) sBeat += snareR[i];   // beats 2&4
    for (int i = 2; i < kBin; i += 4) sOff  += snareR[i];   // offbeat(半速下的 2&4)
    if (sTotal > 1e-6f) backbeat_ += (std::max(sBeat, sOff) / sTotal - backbeat_) * 0.05f;

    int dense = 0;   // 拍起点桶里 kick 显著(>0.4×最强)的个数 → 每拍底鼓
    for (int i = 0; i < kBin; i += 4) if (kickR[i] > 0.4f * kMax + 1e-6f) ++dense;
    kickDensity_ += (dense / 4.0f - kickDensity_) * 0.05f;
}
