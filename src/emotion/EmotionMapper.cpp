#include "emotion/EmotionMapper.h"

#include "dsp/FeatureExtractor.h"

#include <algorithm>

namespace {
// 用户校准的 minorShare 分界:默认 0.30/0.55(标准),setUserCalibration 可个性化。
float g_happyThresh = 0.30f;   // 以下判持续大调(欢快基因)
float g_sadThresh   = 0.55f;   // 以上判持续小调(伤感基因)

constexpr float kHappyDefault = 0.30f;
constexpr float kSadDefault   = 0.55f;
}  // namespace

namespace EmotionMapper {

void setUserCalibration(float happyThresh, float sadThresh) {
    g_happyThresh = std::clamp(happyThresh, 0.02f, 0.93f);
    g_sadThresh = std::clamp(sadThresh, std::max(0.07f, g_happyThresh + 0.05f), 0.98f);
}
void resetCalibration() {
    g_happyThresh = kHappyDefault;
    g_sadThresh = kSadDefault;
}
float happyThresh() { return g_happyThresh; }
float sadThresh()   { return g_sadThresh; }

void calibrateFromMs(float happyMs, float sadMs, float& ht, float& st) {
    // 拉开式:分界放在「欢快歌均值之上、伤感歌均值之下」,让两首锚定歌直接命中各自档。
    //   默认 0.30/0.55 是宽模糊带(通用),校准就是为个人口味收紧 —— 锚点靠得近时带更窄(诚实)。
    const float M = (happyMs + sadMs) * 0.5f;
    ht = std::clamp(happyMs + 0.06f, 0.05f, 0.90f);
    st = std::clamp(sadMs - 0.06f, 0.10f, 0.95f);
    if (st <= ht) {                       // 锚点异常(重叠/标反):退回中点窄带,至少留分界
        ht = std::clamp(M - 0.04f, 0.05f, 0.93f);
        st = std::min(0.95f, ht + 0.08f);
    }
}

// 自动识别分桶:只返回 10 个情绪档(序号 <10)。活动档(Focus/Work/Game/...)由手动
// 模式设置,此处永不返回——未来若要自动判活动档需另行设计,别在这加 case 破坏契约。
Tier mapTier(float valence, float arousal) {
    if (arousal > 0.72f) {
        if (valence > 0.35f)      return Tier::Hype;
        else if (valence > -0.1f) return Tier::Surprised;
        else                      return Tier::Angry;
    }
    if (arousal > 0.60f) {   // 0.60-0.72:高能量(dubstep 等)负 valence → 愤怒
        if (valence > 0.45f)      return Tier::Love;
        else if (valence > 0.10f) return Tier::Joyful;
        else if (valence > -0.25f)return Tier::Surprised;
        else                      return Tier::Angry;
    }
    if (arousal > 0.50f) {   // 0.50-0.60:中高能量(流行小调段)负 valence → 躁动(非愤怒)
        if (valence > 0.45f)      return Tier::Love;
        else if (valence > 0.10f) return Tier::Joyful;
        else if (valence > -0.25f)return Tier::Surprised;
        else                      return Tier::Agitated;
    }
    if (arousal > 0.40f) {   // 活泼(arousal>0.4)才欢快
        if (valence > 0.35f)      return Tier::Joyful;
        else if (valence > -0.10f)return Tier::Healing;
        else if (valence > -0.35f)return Tier::Agitated;
        else                      return Tier::Sad;
    }
    if (arousal > 0.30f) {   // 0.30-0.40:温柔/安静(低 rms 抒情歌)→ 治愈/平静,不欢快
        if (valence > -0.10f)     return Tier::Healing;
        else if (valence > -0.35f)return Tier::Agitated;
        else                      return Tier::Sad;
    }
    if (valence > 0.40f)      return Tier::Healing;
    if (valence > 0.00f)      return Tier::Calm;
    if (valence > -0.35f)     return Tier::Sleepy;
    return Tier::Sad;
}

// 按场景分流:音乐走声学 valence/arousal 模型;人声/噪声走简化映射,
// 这样播客/视频讲解时宠物不乱跳音乐表情,游戏/嘈杂时表现警戒。
Emotion map(const Features& f) {
    Emotion e;
    switch (f.sound) {
        case SoundClass::Silence:
            // 由 PetWidget idle 接管,这里的值不影响(仍给个困倦低位)
            e.valence = 0.10f;
            e.arousal = 0.10f;
            break;
        case SoundClass::Voice: {
            // 人声(播客/讲解/对话):专注聆听,温和中性,arousal 只跟语速能量
            e.valence = 0.25f;
            e.arousal = std::clamp(0.30f + 0.40f * f.rms, 0.0f, 1.0f);
            break;
        }
        case SoundClass::Noise: {
            // 噪声/游戏音效:警戒/兴奋,随能量,略偏负
            e.valence = std::clamp(-0.05f + 0.10f * f.centroid - 0.20f * f.rms, -0.4f, 0.1f);
            e.arousal = std::clamp(0.55f + 0.35f * f.rms, 0.0f, 1.0f);
            break;
        }
        case SoundClass::Music:
        default: {
            // arousal(激烈度):响度 rms + 低频律动 lowRatio 主导,基础抬高(音乐普遍比静默活跃),
            // centroid 次之 —— 避免 EDM 因 centroid 偏低(电子音低沉)被估成低 arousal → 误落 Sleepy/Sad。
            // valence 改由调式 mode 驱动(chroma 抓的「基因」:大调正/小调负)。
            // mode 已含 keyConfidence(低置信→接近 0→valence 中性→落 Calm/Healing,不乱猜)。
            // 旧的外衣估法(centroid/flatness)是混音外衣、不准,弃用。
            float arousal = std::clamp(0.30f + 0.50f * f.rms + 0.22f * f.lowRatio + 0.12f * f.centroid, 0.0f, 1.0f);

            // 扩充判断角度:光看 mode 的"谁赢"不够 ——「相关调同构」(C大调/A小调 chroma
            // 完全一样,如 Am-F-C-G 流行和声)会让 K-K 大调小调五五开 → margin≈0、mode 微弱偏正,
            // 旧公式把它当中性/微弱大调 → 落 Healing/Calm。但低能慢歌这种"调式模糊"其实是
            // 伤感签名(治愈需要清晰大调)。
            //   minorShare = 最近~1.7s 判为小调的帧占比,是唯一判别器(实测 4 首歌):
            //     欢快歌(我的未来不是梦)0.26 <0.30 持续大调;伤感歌(黄昏/幻听)0.39~0.42;
            //     dubstep 0.61。margin 单帧噪声大、只作极端覆盖。
            //   → 持续大调(ms<0.30)正;持续小调(ms>0.55)负;中间模糊:
            //     低能(arousal<0.55)→ 伤感(替代旧的 Calm/Healing 错判);高能 → 按 mode 符号(躁动/惊讶)
            float gene;
            if (f.minorShare < g_happyThresh)            gene = +f.mode;           // 持续大调 → 治愈/欢快(阈值可校准)
            else if (f.minorShare > g_sadThresh)         gene = -f.keyConfidence;  // 持续小调 → 伤感/愤怒(阈值可校准)
            else if (f.keyMargin <= -0.20f)              gene = -f.keyConfidence;  // 模糊但强烈偏小调
            else if (arousal < 0.55f)                    gene = -0.5f;             // 真模糊+低能 → 伤感
            else                                         gene = f.mode;            // 真模糊+高能 → mode 符号
            float valence = std::clamp(gene * 1.3f, -1.0f, 1.0f);
            e.valence = valence;
            e.arousal = arousal;
            break;
        }
    }
    e.tier = mapTier(e.valence, e.arousal);
    return e;
}

Genre guessGenre(const Features& f) {
    // 非音乐场景给中性默认流派(影响背景色/音浪风格)
    if (f.sound == SoundClass::Voice)   return Genre::Pop;    // 中性背景
    if (f.sound == SoundClass::Noise)   return Genre::EDM;    // 亮/活跃背景
    if (f.sound == SoundClass::Silence) return Genre::Ballad; // 低活跃(idle 接管)

    // 音乐:bpm 规律性(置信度)够才按 bpm 判,否则回退 rms/centroid
    if (f.bpmConfidence > 0.25f && f.bpm > 1.0f) {
        if (f.bpm >= 115.0f) return Genre::EDM;
        if (f.bpm < 88.0f)   return Genre::Ballad;
        return Genre::Pop;
    }
    if (f.rms > 0.45f && f.centroid > 0.45f) return Genre::EDM;
    if (f.rms < 0.18f || f.centroid < 0.25f) return Genre::Ballad;
    return Genre::Pop;
}

TierDef def(Tier t) {
    // 每档一行:valence/arousal 定动画节奏(呼吸/嘴动/眨眼快慢),genre 定手动模式的
    // 背景色系与节拍。情绪档的 V/A 与原 fillVA 一致(--dump 现改调 canonical,去重)。
    switch (t) {
        case Tier::Joyful:    return { 0.8f, 0.80f, Genre::Pop,    "欢快"};
        case Tier::Hype:      return { 0.5f, 0.95f, Genre::EDM,    "热血"};
        case Tier::Healing:   return { 0.7f, 0.20f, Genre::Ballad, "治愈"};
        case Tier::Calm:      return { 0.2f, 0.15f, Genre::Ballad, "平静"};
        case Tier::Sad:       return {-0.7f, 0.20f, Genre::Ballad, "伤感"};
        case Tier::Agitated:  return {-0.4f, 0.85f, Genre::EDM,    "躁动"};
        case Tier::Surprised: return { 0.0f, 0.90f, Genre::Pop,    "惊讶"};
        case Tier::Sleepy:    return { 0.1f, 0.12f, Genre::Ballad, "困倦"};
        case Tier::Angry:     return {-0.6f, 0.80f, Genre::EDM,    "愤怒"};
        case Tier::Love:      return { 0.7f, 0.55f, Genre::Pop,    "爱心"};
        // 活动档(手动)
        case Tier::Focus:     return { 0.35f, 0.45f, Genre::Pop,    "专注"};
        case Tier::Work:      return { 0.25f, 0.55f, Genre::Pop,    "工作"};
        case Tier::Game:      return { 0.55f, 0.90f, Genre::EDM,    "游戏"};
        case Tier::Rest:      return { 0.40f, 0.10f, Genre::Ballad, "休息"};
        case Tier::Exercise:  return { 0.60f, 0.85f, Genre::EDM,    "运动"};
        case Tier::Edm:       return { 0.60f, 0.92f, Genre::EDM,    "电音"};
    }
    return {0.0f, 0.3f, Genre::Pop, "?"};   // 不可达,防 -Wreturn-type
}

}  // namespace EmotionMapper
