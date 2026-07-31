#include "dsp/RhythmDetector.h"

#include <algorithm>

void RhythmDetector::feed(float kick, float body, const float* snareBands,
                          float bpm, float bpmConf, float analysisHz) {
    if (analysisHz <= 0.0f) analysisHz = 30.0f;

    // 1-4kHz 四子带 → 宽带共现:≥3 带本帧都有显著通量(>0.3×本帧最强带)= 宽带瞬态(军鼓/掌声)。
    //   人声音节窄带只打 1-2 带。稠密混音整带恒「平」(flatness 没用),但窄带瞬态的
    //   「子带同时性」区分得了军鼓 —— Klapuri 多带 ODF 的轻量版。
    float snare = 0.0f, maxB = 0.0f;
    int nSpike = 0;
    for (int k = 0; k < 4; ++k) {
        snare += snareBands[k];
        if (snareBands[k] > maxB) maxB = snareBands[k];
    }
    for (int k = 0; k < 4; ++k)
        if (snareBands[k] > 0.3f * maxB + 1e-6f) ++nSpike;
    const bool broadband = (nSpike >= 3);

    // 包络:快攻慢衰(压帧噪;snareEnv 用作击打能量度量)
    const float atk = 0.5f, dec = 0.90f;
    if (kick  > kickEnv_)  kickEnv_  += (kick  - kickEnv_)  * atk; else kickEnv_  *= dec;
    if (snare > snareEnv_) snareEnv_ += (snare - snareEnv_) * atk; else snareEnv_ *= dec;

    // 军鼓瞬态 = 双带共现:snare 带(1-4kHz)通量尖峰 且 近 ~50ms 内鼓体带(150-300Hz)有 onset。
    //   鼓体 onset 是军鼓的判别特征 —— 踩镲(无鼓体)/人声辅音(无鼓体 onset)/kick(无噪声带)
    //   全被双带条件滤掉,这是稠密混音分不开的根因解法(调研:Goto 谐波抑制的轻量版)。
    // 鼓体带:通量 > 3×背景且上升 → 置 latch(≈50ms)。
    if (bodyLatch_ > 0.0f) bodyLatch_ -= 1.0f;
    if (body > 3.0f * bodyNoise_ + 0.02f && body > prevBodyFlux_) {
        bodyLatch_ = analysisHz * 0.05f;
    } else {
        bodyNoise_ += (body - bodyNoise_) * 0.05f;   // 未 onset → 背景跟踪
    }
    prevBodyFlux_ = body;

    // snare 带:通量 > 3×背景且上升 且 鼓体 latch 激活 且 snare 强度 ≥0.25×kick 包络
    //   且宽带共现(≥3 子带同时)。kick 纯低频 FFT 泄漏到 snare 带的分量小 → kick 相对量滤掉。
    if (hitCooldown_ > 0.0f) hitCooldown_ -= 1.0f;
    const float fth = 3.0f * fluxNoise_ + 0.02f;
    bool snareHit = false;
    if (hitCooldown_ <= 0.0f && snare > fth && snare > prevSnareFlux_
        && snare > 0.25f * kickEnv_ && bodyLatch_ > 0.0f && broadband) {
        snareHit = true;
        hitCooldown_ = analysisHz * 0.12f;
    } else {
        fluxNoise_ += (snare - fluxNoise_) * 0.05f;   // 未击打 → 背景跟踪
    }
    prevSnareFlux_ = snare;

    // 拍相位累计(bpm 可信才累积,否则直方图只衰减 → 特征归零)
    if (bpm > 1.0f && bpmConf > 0.25f) {
        beatPeriod_ = analysisHz * 60.0f / bpm;
        const float barPeriod = 4.0f * beatPeriod_;
        phaseFrames_ += 1.0f;
        if (phaseFrames_ >= barPeriod) phaseFrames_ -= barPeriod;
        int b = static_cast<int>(phaseFrames_ / barPeriod * kBin);
        if (b >= kBin) b = kBin - 1;
        kickHist_[b] += kickEnv_;
        if (snareHit) snareHitHist_[b] += snareEnv_;   // 真击打才记能量
    }

    // 历史衰减(≈最近 ~16s,反映最近几小节)
    for (int i = 0; i < kBin; ++i) {
        kickHist_[i] *= 0.998f;
        snareHitHist_[i] *= 0.998f;
    }

    // 自旋对齐:最强 kick 相桶 → bin0(downbeat)
    int pivot = 0;
    for (int i = 1; i < kBin; ++i) if (kickHist_[i] > kickHist_[pivot]) pivot = i;
    float kickR[kBin], sHitR[kBin];
    for (int i = 0; i < kBin; ++i) {
        kickR[i]  = kickHist_[(i + pivot) % kBin];
        sHitR[i]  = snareHitHist_[(i + pivot) % kBin];
    }

    // 特征:底鼓密度/切分 用 kick 能量;反拍 用「军鼓击打落点」占比
    float kTotal = 0.0f, kOff = 0.0f, kMax = 0.0f;
    for (int i = 0; i < kBin; ++i) {
        kTotal += kickR[i];
        if (kickR[i] > kMax) kMax = kickR[i];
    }
    for (int i = 2; i < kBin; i += 4) kOff  += kickR[i];
    const float kd = std::max(1e-6f, kTotal);
    syncop_ += (kOff / kd - syncop_) * 0.05f;

    int dense = 0;   // 拍起点桶里 kick 显著(>0.4×最强)的个数 → 每拍底鼓
    for (int i = 0; i < kBin; i += 4) if (kickR[i] > 0.4f * kMax + 1e-6f) ++dense;
    kickDensity_ += (dense / 4.0f - kickDensity_) * 0.05f;

    // 反拍 = 击打落点 2/4 拍占比。BPM 常把 kick 1&3 的 rock 读成半速 → snare 2&4 落到
    // offbeat {2,6,10,14},取「拍位或 offbeat 更大」兜底(两种 tempo 解读都算反拍)。
    // gating:无规律底鼓(ballad/无鼓)→ 没拍可反,归零;击打总量太少 → 防单发噪声。
    float sHitTotal = 0.0f, sHitBeat = 0.0f, sHitOff = 0.0f;
    for (int i = 0; i < kBin; ++i) sHitTotal += sHitR[i];
    sHitBeat = sHitR[4] + sHitR[12];
    sHitOff  = sHitR[2] + sHitR[6] + sHitR[10] + sHitR[14];
    if (kickDensity_ > 0.30f && sHitTotal > 20.0f)
        backbeat_ += (std::max(sHitBeat, sHitOff) / sHitTotal - backbeat_) * 0.05f;
    else
        backbeat_ *= 0.95f;   // 无拍/击打太少 → 衰减回 0
}
