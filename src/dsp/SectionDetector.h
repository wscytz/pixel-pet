#pragma once

#include <vector>

// 歌曲结构段识别(段落):1Hz 粒度,chroma 自相似 + 能量。
//   当前和声 ≈ 8~30s 前出现过(结构级重复)→ 反复段;其中能量高 → 副歌;
//   新和声(novelty 高)→ 桥段;开头 ~12s → 前奏;持续低能 → 尾奏。
// 滞回:候选需持续 ~3s 才采纳,避免每秒乱跳。阈值为起点,靠 analyze 手调。
// 设计注意:主歌/副歌常是「相关调同构」(Am-F-C-G ≡ C-G-Am-F,chroma 几乎一样),
//   光 chroma 分不开主歌/副歌 → 副歌靠「重复 + 更响」双条件,能量是关键一腿。
enum class Section { Unknown, Intro, Verse, Chorus, Bridge, Outro };

inline const char* sectionName(Section s) {
    switch (s) {
        case Section::Unknown: return "?";
        case Section::Intro:   return "Intro";
        case Section::Verse:   return "Verse";
        case Section::Chorus:  return "Chorus";
        case Section::Bridge:  return "Bridge";
        case Section::Outro:   return "Outro";
    }
    return "?";
}

class SectionDetector {
public:
    // 每帧喂 12 维 chroma(平滑后)+ rms;analysisHz(≈30)决定 1Hz 下采样率
    void feed(const float* chroma, float rms, float analysisHz);
    Section section() const { return sec_; }
    float repeat()  const { return repeat_; }   // 0..1 结构级和声重复度(副歌强度信号)
    float novelty() const { return novelty_; }  // 0..1 新和声程度(段边界信号)

private:
    void tick(const float* chroma, float rms);
    float pearson(const float* a, const float* b) const;

    std::vector<float> chromaHist_;   // kHist*12(秒粒度滚动)
    std::vector<float> rmsHist_;      // kHist
    int n_ = 0;                       // 有效秒数(<= kHist)
    float acc_ = 0.0f;                // 距 1Hz tick 的帧累计
    Section sec_ = Section::Unknown;
    int dissent_ = 0;                 // 滞回计数(3 tick = 3s)
    float repeat_ = 0.0f, novelty_ = 0.0f;
    float energyEnv_ = 0.0f;          // 帧级 EMA 平滑能量(~0.7s 时间常数,压掉节拍/音符尖峰)
    static constexpr int kHist = 48;  // 48s 滚动历史
};
