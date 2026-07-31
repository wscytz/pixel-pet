#pragma once

#include <QColor>
#include "emotion/Emotion.h"
#include "emotion/EmotionMapper.h"
#include "emotion/Trajectory.h"
#include "ui/Palette.h"

// 每帧渲染状态。流派定色系/节拍,情绪定颜表情。
struct FrameState {
    Tier tier = Tier::Calm;
    Genre genre = Genre::Pop;
    QColor color;       // 当前主色(跟流派,跨档平滑过渡)
    QColor accent;      // 频谱色
    float facePhase = 0.0f;
    float blink = 0.0f;
    float mouthOpen = 0.0f;
    float beatPulse = 0.0f;   // 节拍脉冲 0..1(每拍开头冲起)
};

class PetAnimator {
public:
    void setEmotion(const Emotion& e);
    void setGenre(Genre g);
    void setIdle(bool b);               // 静默 idle(没声音时打瞌睡)
    void setManual(bool on, Tier t);    // 手动模式:固定显示某档(压过自动/idle)
    void setBeat(float b);              // 外部踩拍能量(>=0 启用,<0 回退假 bpm)
    void setSectionBoost(float b);      // 结构段表现力 0..1(副歌 1.0 踩拍更猛/嘴张更大,Intro/Outro 收着)
    void setDance(float d);             // 律动密度 0..1(每拍底鼓数:EDM 四拍=1,抒情≈0 → 弹跳幅度)
    void resetSwitch();                 // 中断后恢复:清滞回,允许立即重判(连续播放才粘滞)
    void snap(const Emotion& e, Genre g);   // 直接跳到目标(导出静态帧用)
    void tick(int dtMs);

    const FrameState& state() const { return st_; }

private:
    Emotion target_;
    Genre genre_ = Genre::Pop;
    float valence_ = 0.0f;
    float arousal_ = 0.3f;
    QColor color_  = QColor(255, 210, 63);
    QColor accent_ = QColor(255, 111, 181);
    float tMs_ = 0.0f;
    float lastTierSwitchMs_ = -10000.0f;   // tier 滞回:上次切换时间
    bool idle_ = false;                    // 静默 idle
    bool manual_ = false;                  // 手动模式(固定 manualTier_)
    Tier manualTier_ = Tier::Calm;
    Tier prevTier_ = Tier::Calm;           // 自动模式上一个档(防来回横跳)
    bool blinkOnce_ = false;               // 换档时单次眨眼掩饰
    float extBeat_ = 0.0f;                 // 外部踩拍能量
    bool hasExtBeat_ = false;              // 是否使用外部踩拍(否则假 bpm)
    float sectionBoost_ = 0.5f;            // 结构段表现力(平滑后)
    float sectionBoostTarget_ = 0.5f;      // 目标(副歌 1.0 / Intro·Outro 0.15)
    float dance_ = 0.0f;                   // 律动密度(底鼓)
    TrajectoryTracker traj_;               // 情绪轨迹(当前 vs 慢参考)
    float build_ = 0.5f;                   // 蓄势期待感(0..1,driftA 升 → 大)
    FrameState st_;
};
