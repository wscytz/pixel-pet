#pragma once

// 节奏型识别:kick(低频 ~40-120Hz)/ snare(中频 ~1-4kHz)通量 → 包络 → 16 相桶直方图。
//   拍相位由 bpm 驱动(bpmConf 低则不累计,特征归零);直方图按「最强 kick 相桶」
//   自旋对齐到 downbeat(假设主拍有 kick,音乐通识)。输出 0..1:
//     backbeat   snare 落 2/4 拍占比      —— rock/pop 反拍
//     kickDensity 每拍 kick 数            —— EDM 四拍=1.0,rock 1&3=0.5,抒情≈0
//     syncop     offbeat kick 能量占比     —— dubstep/hiphop 切分
// 阈值/衰减是起点,靠 analyze 实测手调(和 SectionDetector 同一套方法学)。
class RhythmDetector {
public:
    // 每帧喂 kick/body 通量 + 1-4kHz 四子带通量 + bpm/bpmConf + 帧率
    void feed(float kickFlux, float bodyFlux, const float* snareBands,
              float bpm, float bpmConf, float analysisHz);
    float backbeat() const { return backbeat_; }
    float kickDensity() const { return kickDensity_; }
    float syncop() const { return syncop_; }

private:
    static constexpr int kBin = 16;   // 一小节(4 拍)16 相桶
    float kickEnv_ = 0.0f, snareEnv_ = 0.0f;
    float kickHist_[kBin] = {}, snareHitHist_[kBin] = {};
    float fluxNoise_ = 0.0f;          // 背景 snare 通量(未击打时 EMA,burst 冻结)
    float prevSnareFlux_ = 0.0f;      // 上帧通量(判上升尖峰)
    float hitCooldown_ = 0.0f;        // 防连发(帧数)
    float bodyNoise_ = 0.0f;          // 鼓体带(150-300Hz)背景通量(未 onset 时 EMA)
    float prevBodyFlux_ = 0.0f;       // 上帧鼓体通量(判上升)
    float bodyLatch_ = 0.0f;          // 鼓体 onset 激活窗口(帧数,~50ms);snare 尖峰须共现才算军鼓
    float phaseFrames_ = 0.0f;
    float beatPeriod_ = 0.0f;
    float backbeat_ = 0.0f, kickDensity_ = 0.0f, syncop_ = 0.0f;
};
