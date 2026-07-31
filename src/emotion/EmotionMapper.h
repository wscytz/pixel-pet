#pragma once

#include "emotion/Emotion.h"

struct Features;

// 特征 → 情绪档 + 流派。纯规则映射,权重是起点,实测手调。
namespace EmotionMapper {
// 声学特征 → 情绪档 + 流派(纯规则,无 ML)。抖动由 PetAnimator 的平滑+滞回+防抖抑制;
// 但音频本身难以可靠区分「打游戏 vs 学习」等活动场景,故活动状态以手动切换为主。
Emotion map(const Features& f);
Tier mapTier(float valence, float arousal);   // 分桶(供 PetAnimator 滞回用)
Genre guessGenre(const Features& f);

// 每档规范定义:valence/arousal(定动画节奏)+ 档配流派(手动模式)+ 中文名(菜单)。
// 手动模式与 --dump 共用这一张表,避免各处各写。def() 穷举 switch 无 default →
// -Werror=switch 兜住漏档。
struct TierDef { float valence; float arousal; Genre genre; const char* name; };
TierDef def(Tier t);
inline Emotion canonical(Tier t) { const TierDef d = def(t); return Emotion{d.valence, d.arousal, t}; }
}
