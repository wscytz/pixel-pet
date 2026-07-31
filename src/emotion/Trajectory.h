#pragma once

#include <algorithm>
#include <cmath>

// 情绪轨迹:当前情绪 vs 慢参考(近 ~3s 基线)之差 = 近期运动方向。
//   driftV>0 升华(往积极走)/ driftV<0 失落;driftA>0 蓄势(能量在涨)/ <0 放松。
// 2Hz 采样 + 慢 EMA 参考(α=0.15,TC≈3.3s),抗单帧噪声。analyze 与 GUI 共用同一逻辑。
class TrajectoryTracker {
public:
    // 每帧喂当前 valence/arousal + 帧率(30/60);内部按 2Hz 采样
    void feed(float valence, float arousal, float analysisHz) {
        if (analysisHz <= 0.0f) analysisHz = 30.0f;
        acc_ += 1.0f;
        const float step = analysisHz / kRate;
        if (acc_ < step) return;
        acc_ -= step;
        if (!first_) {                    // 首采样只建参考,不产生漂移(防启动假信号)
            refV_ = valence; refA_ = arousal;
            driftV_ = 0.0f; driftA_ = 0.0f;
            first_ = true;
            return;
        }
        driftV_ = valence - refV_;        // 先算差(旧基线)再更新参考
        driftA_ = arousal - refA_;
        refV_ = refV_ * 0.85f + valence * 0.15f;
        refA_ = refA_ * 0.85f + arousal * 0.15f;
    }
    float driftV() const { return driftV_; }   // -1..+1 情绪方向
    float driftA() const { return driftA_; }
    float momentum() const {                    // 0..1 运动强度
        return std::clamp(std::fabs(driftV_) * 1.2f + std::fabs(driftA_) * 1.2f, 0.0f, 1.0f);
    }

private:
    static constexpr float kRate = 2.0f;   // 采样 2Hz
    float refV_ = 0.0f, refA_ = 0.0f;
    float driftV_ = 0.0f, driftA_ = 0.0f;
    float acc_ = 0.0f;
    bool first_ = false;
};
