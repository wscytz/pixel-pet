#include "dsp/SectionDetector.h"

#include <algorithm>
#include <cmath>

void SectionDetector::feed(const float* chroma, float rms, float analysisHz) {
    if (analysisHz <= 0.0f) analysisHz = 30.0f;
    // 帧级能量平滑:瞬态 rms 被节拍/音符截断打得很尖(如鼓点 0.5 间奏 0.05),
    // 直接比会来回触发段切换。EMA ~0.7s 时间常数(α=0.05@30fps)压掉高频抖动。
    energyEnv_ += (rms - energyEnv_) * 0.05f;
    acc_ += 1.0f;
    if (acc_ >= analysisHz) { acc_ -= analysisHz; tick(chroma, energyEnv_); }
}

float SectionDetector::pearson(const float* a, const float* b) const {
    double ma = 0, mb = 0;
    for (int i = 0; i < 12; ++i) { ma += a[i]; mb += b[i]; }
    ma /= 12; mb /= 12;
    double sxy = 0, sxx = 0, syy = 0;
    for (int i = 0; i < 12; ++i) { const double da = a[i] - ma, db = b[i] - mb; sxy += da * db; sxx += da * da; syy += db * db; }
    const double d = std::sqrt(sxx * syy);
    return d > 0 ? static_cast<float>(sxy / d) : 0.0f;
}

void SectionDetector::tick(const float* chroma, float rms) {
    if (static_cast<int>(chromaHist_.size()) >= kHist * 12) {   // 滚动:丢最旧 1s
        chromaHist_.erase(chromaHist_.begin(), chromaHist_.begin() + 12);
        rmsHist_.erase(rmsHist_.begin());
        --n_;
    }
    for (int c = 0; c < 12; ++c) chromaHist_.push_back(chroma[c]);
    rmsHist_.push_back(rms);
    ++n_;

    if (n_ < 8) { sec_ = Section::Unknown; return; }            // 历史不足(前 ~8s)

    // 相似度扫描:当前 vs 2~6s 前(乐句级重复 → novelty 低)、8~30s 前(结构级重复 → repeat 高)
    const int now = n_ - 1;
    const float* cur = &chromaHist_[static_cast<size_t>(now) * 12];
    float simRecent = -2.0f, simRepeat = -2.0f;
    for (int lag = 2; lag <= 6 && now - lag >= 0; ++lag)
        simRecent = std::max(simRecent, pearson(cur, &chromaHist_[static_cast<size_t>(now - lag) * 12]));
    for (int lag = 8; lag <= 30 && now - lag >= 0; ++lag)
        simRepeat = std::max(simRepeat, pearson(cur, &chromaHist_[static_cast<size_t>(now - lag) * 12]));
    novelty_ = std::clamp(1.0f - std::max(0.0f, simRecent), 0.0f, 1.0f);
    repeat_  = std::clamp(std::max(0.0f, simRepeat), 0.0f, 1.0f);

    // 能量基线:中位数抗副歌通胀(歌里响段多时 p65 会被顶上去,第二个副歌就"不够响";
    // 中位数稳在主歌水平)。energyHigh 用「相对中位的倍数」,比绝对分位+偏置稳。
    std::vector<float> s = rmsHist_;
    std::sort(s.begin(), s.end());
    const float p20 = s[s.size() * 20 / 100];
    const float med = s[s.size() / 2];
    const bool energyHigh = rms > 1.20f * med;   // 抒情歌副歌只比主歌响 ~1.2-1.3×,1.35 太严
    const bool energyLow  = rms < p20 - 0.01f;
    const bool windDown   = rms < 0.5f * med;   // 最近能量显著低于全曲中位 → 收尾

    Section cand;
    if (n_ < 12)                                    cand = Section::Intro;       // 开头默认前奏
    else if (n_ >= 20 && repeat_ > 0.60f && energyHigh)  cand = Section::Chorus;  // 重复+更响+有上下文 → 副歌
    else if (repeat_ > 0.60f)                       cand = Section::Verse;       // 重复 + 中等 → 主歌
    else if (novelty_ > 0.50f && !energyLow)        cand = Section::Bridge;      // 新和声 → 桥段
    else if (energyLow || windDown)                 cand = Section::Outro;       // 持续低能/收尾 → 尾奏
    else                                            cand = Section::Verse;

    if (cand != sec_) { if (++dissent_ >= 3) { sec_ = cand; dissent_ = 0; } }
    else dissent_ = 0;
}
