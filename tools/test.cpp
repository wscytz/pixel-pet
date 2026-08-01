// pp_test — 纯算法单元测试(无 Qt,无框架,assert 式)。
//   跑:./pp_test → 全过 exit 0,有失败 exit 1。CI Regression 步骤调用。
//   覆盖:FFT 峰值定位、mapTier 分桶、EmotionMapper 调式→valence 方向性、用户校准。
//   不测 FeatureExtractor::compute(需音频数据),只测无副作用纯函数 + 手构造 Features。
#include "dsp/FFT.h"
#include "dsp/FeatureExtractor.h"
#include "emotion/Emotion.h"
#include "emotion/EmotionMapper.h"

#include <complex>
#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0, g_total = 0;
static void check(bool ok, const char* name) {
    std::fprintf(stderr, "  %s %s\n", ok ? "ok  " : "FAIL", name);
    ++g_total;
    if (!ok) ++g_fail;
}

// 手构造一个 Music 场景的 Features(跳过 FFT,直接填字段测映射逻辑)
static Features music(float mode, float minorShare, float keyConf = 0.8f,
                      float rms = 0.30f, float lowRatio = 0.40f, float centroid = 0.40f) {
    Features f{};
    f.sound = SoundClass::Music;
    f.mode = mode;
    f.minorShare = minorShare;
    f.keyConfidence = keyConf;
    f.keyMargin = 0.0f;
    f.rms = rms;
    f.lowRatio = lowRatio;
    f.centroid = centroid;
    return f;
}

int main() {
    std::fprintf(stderr, "[FFT] 正弦波峰值定位\n");
    {
        constexpr int N = 2048;
        const float sr = 44100.0f, freq = 440.0f;
        std::vector<std::complex<float>> buf(N);
        for (int i = 0; i < N; ++i)
            buf[i] = std::sin(2.0f * 3.14159265f * freq * i / sr);
        FFT::forward(buf.data(), N);
        int peak = 1; float mx = 0.0f;
        for (int i = 1; i < N / 2; ++i)
            if (std::abs(buf[i]) > mx) { mx = std::abs(buf[i]); peak = i; }
        const float peakHz = peak * sr / N;
        check(std::fabs(peakHz - freq) < 2.0f * sr / N, "440Hz 正弦 → 峰值落在对应频率");
    }

    std::fprintf(stderr, "[mapTier] 分桶\n");
    {
        check(EmotionMapper::mapTier(+0.8f, +0.8f) == Tier::Hype, "高 valence 高 arousal → Hype");
        check(EmotionMapper::mapTier(-0.7f, +0.2f) == Tier::Sad, "强负 valence 低 arousal → Sad");
        check(EmotionMapper::mapTier(+0.2f, +0.15f) == Tier::Calm, "微正 valence 极低 arousal → Calm");
    }

    std::fprintf(stderr, "[map] 调式 → valence 方向性\n");
    {
        const Emotion eMaj = EmotionMapper::map(music(+0.8f, 0.20f));              // 持续大调
        check(eMaj.valence > 0.3f, "持续大调(mode+0.8, ms0.20) → valence 正");
        const Emotion eMin = EmotionMapper::map(music(-0.8f, 0.70f, 0.8f, 0.10f));  // 持续小调低能
        check(eMin.valence < -0.3f, "持续小调(ms0.70, 低能) → valence 负");
    }

    std::fprintf(stderr, "[map] 偏大带 + 清晰大调 → 欢快(励志歌修复)\n");
    {
        const Emotion eStrong = EmotionMapper::map(music(+0.6f, 0.40f, 0.8f, 0.10f));  // ms0.40 偏大带 + mode+0.6 清晰大调
        check(eStrong.valence > 0.0f, "偏大带(ms0.40)+清晰大调(mode+0.6) → valence 正(不误判伤感)");
        const Emotion eWeak = EmotionMapper::map(music(+0.10f, 0.40f, 0.8f, 0.10f));    // ms0.40 偏大带 + 弱 mode
        check(eWeak.valence < 0.0f, "偏大带(ms0.40)+弱 mode(+0.10) → valence 负(伤感歌保住)");
    }

    std::fprintf(stderr, "[calibration] 用户校准改变分界\n");
    {
        EmotionMapper::resetCalibration();
        const Features f = music(+0.15f, 0.35f, 0.8f, 0.10f);   // 弱大调 mode+0.15,低能,ms0.35(偏大带)
        const Emotion e0 = EmotionMapper::map(f);               // 默认:mode 弱(<0.25)不触发偏大带欢快 → 模糊+低能 → 伤感
        EmotionMapper::setUserCalibration(0.40f, 0.55f);        // happyThresh 提到 0.40
        const Emotion e1 = EmotionMapper::map(f);               // 0.35<0.40 → 持续大调 → 正
        check(e1.valence > e0.valence, "提高 happyThresh:ms0.35 从模糊伤感变持续大调,valence 升");
        EmotionMapper::resetCalibration();
    }

    std::fprintf(stderr, "[calibrateFromMs] 推荐分界\n");
    {
        float ht = 0.0f, st = 0.0f;
        EmotionMapper::calibrateFromMs(0.25f, 0.50f, ht, st);   // 欢快 0.25 / 伤感 0.50
        check(ht > 0.25f && ht < 0.40f, "happyThresh 落在欢快均值之上");
        check(st < 0.50f && st > 0.35f, "sadThresh 落在伤感均值之下");
        check(st > ht, "sadThresh > happyThresh(分界有序)");
    }

    constexpr int kExpected = 12;   // FFT1 + mapTier3 + map2 + 偏大带2 + calibration1 + calibrateFromMs3;加/删断言时同步更新(防整块没跑也报"全部通过")
    if (g_fail == 0 && g_total == kExpected) { std::fprintf(stderr, "\n=== 全部通过(%d 断言)===\n", g_total); return 0; }
    if (g_fail == 0) std::fprintf(stderr, "\n=== 断言数不符:跑了 %d,预期 %d ===\n", g_total, kExpected);
    std::fprintf(stderr, "\n=== %d 项失败 ===\n", g_fail);
    return 1;
}
