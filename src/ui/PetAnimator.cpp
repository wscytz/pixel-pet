#include "ui/PetAnimator.h"

#include <cmath>

using Palette::GenreTheme;

namespace {

QColor lerpC(const QColor& a, const QColor& b, float k) {
    if (k < 0.0f) k = 0.0f;
    if (k > 1.0f) k = 1.0f;
    return QColor(
        static_cast<int>(a.red()   + (b.red()   - a.red())   * k),
        static_cast<int>(a.green() + (b.green() - a.green()) * k),
        static_cast<int>(a.blue()  + (b.blue()  - a.blue())  * k));
}

}  // namespace

void PetAnimator::setEmotion(const Emotion& e) { target_ = e; }
void PetAnimator::setGenre(Genre g) { genre_ = g; }
void PetAnimator::setIdle(bool b) { idle_ = b; }

void PetAnimator::setManual(bool on, Tier t) {
    manual_ = on;
    manualTier_ = t;
    blinkOnce_ = true;                       // 切入/切出都眨眼掩饰换脸
    if (!on) lastTierSwitchMs_ = tMs_ - 1500.0f;  // 切回自动:允许立即重判
}

void PetAnimator::setBeat(float b) {
    if (b < 0.0f) { hasExtBeat_ = false; extBeat_ = 0.0f; }
    else          { hasExtBeat_ = true;  extBeat_ = b; }
}

void PetAnimator::setSectionBoost(float b) {
    sectionBoostTarget_ = b < 0.0f ? 0.0f : (b > 1.0f ? 1.0f : b);
}

void PetAnimator::setDance(float d) {
    dance_ = d < 0.0f ? 0.0f : (d > 1.0f ? 1.0f : d);
}

void PetAnimator::resetSwitch() {
    // 音乐中断后恢复:清滞回计时 + 把当前档记为 prev,允许立即重判(中断不粘滞)。
    // 与「连续播放中的抖动吃粘滞」区分 —— 切歌/暂停/断流后重新检测不被压。
    lastTierSwitchMs_ = tMs_ - 100000.0f;
    prevTier_ = st_.tier;
}

void PetAnimator::snap(const Emotion& e, Genre g) {
    target_  = e;
    genre_   = g;
    valence_ = e.valence;
    arousal_ = e.arousal;
    color_   = Palette::tierColor(e.tier);
    accent_  = Palette::tierAccent(e.tier);
    st_.tier   = e.tier;
    st_.genre  = g;
    st_.color  = color_;
    st_.accent = accent_;
    st_.blink     = 0.0f;
    st_.mouthOpen = 0.5f;
    st_.facePhase = 0.0f;
    st_.beatPulse = 0.0f;
    manual_ = false;            // 静态帧不被手动态遮蔽
    hasExtBeat_ = false;
}

void PetAnimator::tick(int dtMs) {
    tMs_ += static_cast<float>(dtMs);

    valence_ += (target_.valence - valence_) * 0.05f;
    arousal_  += (target_.arousal  - arousal_)  * 0.05f;
    sectionBoost_ += (sectionBoostTarget_ - sectionBoost_) * 0.04f;   // 段表现力平滑(切换不突跳)

    // 情绪轨迹:当前 vs 慢参考(近 ~3s 基线)。音乐在往哪走 → 期待感/呼吸
    traj_.feed(valence_, arousal_, 60.0f);
    const float momentum = traj_.momentum();
    float b = 0.5f + traj_.driftA() * 2.0f;
    build_ = b < 0.0f ? 0.0f : (b > 1.0f ? 1.0f : b);

    const GenreTheme& th = Palette::theme(genre_);

    // 1) 决定本帧显示的 tier:手动 > 自动滞回(idle 在下面单独处理)
    Tier disp = st_.tier;
    if (manual_) {
        disp = manualTier_;
    } else if (!idle_) {
        // tier 滞回 + 防抖:平滑 V/A 重判;普通切换 ≥1.5s 冷却,「来回横跳」(回到刚离开的档)
        // 加到 3s 死区,压低边界抖动。诚实局限:音频难以可靠区分打游戏/学习,故活动状态以手动为主。
        const Tier desired = EmotionMapper::mapTier(valence_, arousal_);
        const float gate = (desired == prevTier_) ? 6000.0f : 4000.0f;   // 粘滞加宽:回切6s / 普通4s(压抖动)
        if (desired != st_.tier && (tMs_ - lastTierSwitchMs_ > gate)) {
            prevTier_ = st_.tier;
            disp = desired;
            lastTierSwitchMs_ = tMs_;
            blinkOnce_ = true;
        }
    }

    // 2) 颜色/辅色朝「显示档」lerp —— 修掉旧版 ≤1.5s 颜色/脸不同步
    color_  = lerpC(color_,  Palette::tierColor(disp),  0.08f);
    accent_ = lerpC(accent_, Palette::tierAccent(disp), 0.08f);

    // 2b) 混合情感:主档脸 + 旧档色微渗。换档后 6s 内把「刚离开的档」颜色轻微渗入当前,
    //     —— 伤感→欢快 的过渡期带一点旧色,表达"混合"而不改判档。
    //     纯展示层色混,不依赖第二个分类器(判得准不准不影响主档),见 #65。
    if (!manual_ && !idle_) {
        const float sinceSwitch = tMs_ - lastTierSwitchMs_;
        if (sinceSwitch >= 0.0f && sinceSwitch < 6000.0f) {
            const float bleed = (1.0f - sinceSwitch / 6000.0f) * 0.35f;
            if (bleed > 0.0f) {
                color_  = lerpC(color_,  Palette::tierColor(prevTier_),  bleed * 0.3f);
                accent_ = lerpC(accent_, Palette::tierAccent(prevTier_), bleed * 0.3f);
            }
        }
    }

    // 3) 动画参数
    float beat = 0.0f;
    if (idle_ && !manual_) {
        // 静默打瞌睡(手动模式不睡):慢眨眼、固定呼吸、无踩拍
        color_  = lerpC(color_,  Palette::tierColor(Tier::Sleepy),  0.05f);
        accent_ = lerpC(accent_, Palette::tierAccent(Tier::Sleepy), 0.05f);
        disp = Tier::Sleepy;
        st_.facePhase = std::fmod(tMs_ / 1200.0f, 1.0f);
        st_.mouthOpen = 0.3f;
        const float ib = std::fmod(tMs_ / 3500.0f, 1.0f);   // 3.5s 慢眨眼
        st_.blink = (ib < 0.06f) ? 1.0f : 0.0f;
    } else {
        float period = 900.0f - 520.0f * arousal_;
        if (period < 260.0f) period = 260.0f;
        period *= (1.0f - 0.12f * momentum);   // 音乐在运动 → 呼吸加快(期待峰值)
        st_.facePhase = std::fmod(tMs_ / period, 1.0f);
        if (st_.facePhase < 0.0f) st_.facePhase += 1.0f;
        const float bp = std::fmod(tMs_ / 3500.0f, 1.0f);
        st_.blink = (bp < 0.035f) ? 1.0f : 0.0f;
        const float sp = 0.004f * (0.5f + arousal_);
        st_.mouthOpen = 0.5f + 0.5f * (0.7f + 0.6f * sectionBoost_) * (0.85f + 0.3f * build_) * std::sin(tMs_ * sp);
        // 踩拍:真实音频能量优先(手动也随真曲跳),否则假 bpm 节拍器
        const float beatMs = 60000.0f / th.bpm;
        const float bphase = std::fmod(tMs_, beatMs) / beatMs;
        const float atk = (th.bpm > 100.0f) ? 0.12f : 0.25f;
        const float fakeBeat = (bphase < atk) ? (1.0f - bphase / atk) : 0.0f;
        beat = hasExtBeat_ ? extBeat_ : fakeBeat;
        // 副歌表现力 + 律动密度 + 情绪动量:踩拍幅度(Intro/Outro 收着;无底鼓轻;音乐在涨更带劲)
        beat *= (0.65f + 0.5f * sectionBoost_) * (0.85f + 0.3f * dance_) * (0.9f + 0.2f * momentum);
    }
    if (blinkOnce_) { st_.blink = 1.0f; blinkOnce_ = false; }

    st_.tier      = disp;
    st_.genre     = genre_;
    st_.color     = color_;
    st_.accent    = accent_;
    st_.beatPulse = beat;
}
