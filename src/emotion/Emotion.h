#pragma once

// 情绪档枚举 + valence/arousal 结构。被 PixelFaces / Palette / PetAnimator /
// (后续)EmotionMapper 共用,放 emotion/ 下避免循环依赖。

enum class Tier {
    Joyful,    // 欢快 ^^
    Hype,      // 热血 ﾟ∀ﾟ
    Healing,   // 治愈 ‿
    Calm,      // 平静 ··
    Sad,       // 伤感 ；；
    Agitated,  // 躁动 ＞＜
    Surprised, // 惊讶 O_O
    Sleepy,    // 困倦 -.-
    Angry,     // 愤怒 ╬╬
    Love,      // 爱心 *^3^*
    // ── 活动档(序号 ≥10):手动模式专用,EmotionMapper::mapTier 的自动识别永不返回 ──
    Focus,     // 专注
    Work,      // 工作
    Game,      // 游戏
    Rest,      // 休息
    Exercise,  // 运动
    Edm,       // 电音(rave 脸 ⊙∀⊙ 举手⚡,荧光绿)
};

inline constexpr int kTierCount = 16;

// 音乐流派:比情绪更高的维度,定整体氛围(色系/节奏/频谱风格)。
enum class Genre { EDM, Ballad, Pop };
inline constexpr int kGenreCount = 3;

// valence-arousal 二维情感(Russell circumplex)。
//   valence ∈ [-1, +1]  伤感 → 欢快
//   arousal  ∈ [ 0,  1] 平静 → 躁动
struct Emotion {
    float valence = 0.0f;
    float arousal = 0.3f;
    Tier tier = Tier::Calm;
};
