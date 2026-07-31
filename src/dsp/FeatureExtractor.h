#pragma once

#include <array>
#include <vector>
#include <cstdint>

// 音频内容类型(场景识别):决定走哪套情绪解读。
//   Silence 静音   Music 音乐   Voice 人声语音(播客/讲解)   Noise 噪声/游戏音效
enum class SoundClass { Silence, Music, Voice, Noise };

// 音频特征:PCM 窗口 或 已算好的频谱 bins → 频谱/能量/重心/BPM/场景。
struct Features {
    std::array<float, 48> spectrum = {};   // 48 频段(log),0..1,喂 PetWidget 音浪
    float rms = 0.0f;        // 时域能量 0..1
    float centroid = 0.0f;   // 频谱重心(归一)0..1,越大越亮
    float bpm = 0.0f;        // 估算 BPM(0 = 还没攒够 onset 历史)
    float bpmConfidence = 0.0f;  // 拍子规律性(归一自相关峰)0..1,低=别信 bpm
    float flatness = 0.0f;   // 谱平坦度 0..1,高=宽带噪声(游戏音效/杂音)
    float lowRatio = 0.0f;   // 低频(<=250Hz)能量占比 0..1,高=有贝斯/鼓(音乐)
    float mode = 0.0f;        // 调式倾向:-1 小调 .. +1 大调(chroma + key profile 估,valence 的"基因")
    float keyConfidence = 0.0f;  // 调式判定置信度(Pearson 相关 0..1,低=别太信 mode)
    float keyMargin = 0.0f;   // 大调最佳 Pearson − 小调最佳 Pearson(-1..+1):
                              //   「相关调同构」(C大调/A小调 chroma 一致)时 ≈0 → 调式模糊(伤感签名)
    float minorShare = 0.0f;  // 最近 ~1.7s 判为小调的帧占比 0..1(时间维度的调式倾向)
    SoundClass sound = SoundClass::Music;   // 场景判别结果
};

class FeatureExtractor {
public:
    // 本地文件音源:PCM 窗口 → FFT → 全特征
    void compute(const float* mono, int n, int sampleRate, float analysisHz, Features& out);
    // 网页音源:已算好的频谱 bins(0..255)+ rms → 全特征(不重 FFT)
    void computeFromBins(const uint8_t* bins, int n, float rms, Features& out);

    // 调式诊断:上一帧 key 估计的完整信息(mode 只用"谁赢",这里保留大小调各自
    // 最佳 Pearson 与胜负差距 margin)。margin 是「大调赢小调多少」的分级量:
    // 相关调(C 大调 vs A 小调)chroma 相同时 margin≈0,清晰大调为正、小调为负。
    // 供离线调参/辅助判据用,生产情绪映射目前仍走 mode。
    struct KeyDiag {
        float majorBest = 0.0f;  // 大调最佳 Pearson(跨 12 旋转)
        float minorBest = 0.0f;  // 小调最佳 Pearson
        int tonic = 0;           // 胜者旋转的 tonic 半音(0=C)
        bool major = false;      // 胜者是否大调
        float margin = 0.0f;     // majorBest - minorBest(正=偏大调,负=偏小调)
    };
    const KeyDiag& lastKeyDiag() const { return keyDiag_; }

private:
    static constexpr int kN = 2048;   // FFT 窗(4096 改变真实音乐 chroma 分布,回退;纯弦苛刻测试不足信)
    std::array<float, 48> prevSpec_ = {};        // 48 段时序平滑
    std::array<float, 12> prevChroma_ = {};      // chroma(12 半音)时序平滑 → 调式估计
    KeyDiag keyDiag_;                            // 上一帧调式诊断(lastKeyDiag 用)
    float minorShare_ = 0.0f;                    // 小调帧占比的滚动平均(α=0.02,~1.7s)
    SoundClass prevSound_ = SoundClass::Music;   // 场景平滑(抗 EDM 合成段 flatness 越界导致的 Music↔Noise 抖动)
    int soundDissent_ = 0;                        // 连续与 prevSound_ 不一致的帧数
    std::vector<float> prevMag_;                 // 上帧幅度(compute 用)
    std::vector<float> prevBinsMag_;             // 上帧幅度(computeFromBins 用)
    std::vector<float> onsetHistory_;            // 低频 onset 包络历史(喂 BPM)
    std::vector<float> bpmHistory_;              // 最近 BPM 估计(中值滤波抗野值)
    float prevBpm_ = 0.0f;

    void updateBpm(float analysisHz, Features& out);  // onsetHistory → BPM 自相关
};
