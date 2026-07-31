#include "ui/Palette.h"

namespace Palette {

QColor tierColor(Tier t) {
    switch (t) {
        case Tier::Joyful:    return QColor(255, 210,  74);   // 暖黄
        case Tier::Hype:      return QColor(255,  90,  44);   // 红橙
        case Tier::Healing:   return QColor( 91, 224, 176);   // 浅绿青
        case Tier::Calm:      return QColor(110, 140, 174);   // 灰蓝
        case Tier::Sad:       return QColor( 74, 110, 168);   // 深蓝
        case Tier::Agitated:  return QColor(200,  74, 224);   // 红紫
        case Tier::Surprised: return QColor(  0, 220, 220);   // 亮青
        case Tier::Sleepy:    return QColor(150, 120, 210);   // 暗紫
        case Tier::Angry:     return QColor(225,  55,  55);   // 深红
        case Tier::Love:      return QColor(255,  90, 150);   // 粉红
        case Tier::Focus:     return QColor( 86, 176, 222);   // 清蓝
        case Tier::Work:      return QColor(130, 142, 180);   // 钢灰蓝
        case Tier::Game:      return QColor(168,  84, 236);   // 电紫
        case Tier::Rest:      return QColor(150, 124, 196);   // 暮紫
        case Tier::Exercise:  return QColor(250, 116,  72);   // 活力橙
        case Tier::Edm:       return QColor(120, 255,  70);   // 荧光绿(acid neon)
    }
    return QColor(200, 200, 200);
}

QColor tierAccent(Tier t) {
    switch (t) {
        case Tier::Joyful:    return QColor(255, 175,  60);
        case Tier::Hype:      return QColor(255,  60,  90);
        case Tier::Healing:   return QColor( 60, 200, 220);
        case Tier::Calm:      return QColor( 90, 120, 200);
        case Tier::Sad:       return QColor(110, 150, 220);
        case Tier::Agitated:  return QColor(160,  60, 220);
        case Tier::Surprised: return QColor( 80, 255, 255);
        case Tier::Sleepy:    return QColor(180, 140, 255);
        case Tier::Angry:     return QColor(255,  90,  90);
        case Tier::Love:      return QColor(255, 130, 180);
        case Tier::Focus:     return QColor(150, 220, 255);
        case Tier::Work:      return QColor(180, 190, 220);
        case Tier::Game:      return QColor( 96, 240, 168);
        case Tier::Rest:      return QColor(206, 168, 150);
        case Tier::Exercise:  return QColor(255, 182,  96);
        case Tier::Edm:       return QColor(  0, 220, 255);   // 电光青
    }
    return QColor(150, 150, 150);
}

QColor tierSignature(Tier t) {
    switch (t) {
        case Tier::Joyful:    return QColor(255, 120, 170);  // 粉颊
        case Tier::Hype:      return QColor(255, 224, 130);  // 金火花
        case Tier::Healing:   return QColor(255, 140, 180);  // 粉颊
        case Tier::Calm:      return QColor(255, 255, 255);
        case Tier::Sad:       return QColor(110, 200, 255);  // 青泪
        case Tier::Agitated:  return QColor(140, 210, 255);  // 青汗
        case Tier::Surprised: return QColor(255, 255, 255);
        case Tier::Sleepy:    return QColor(180, 200, 255);  // 淡蓝 Zzz
        case Tier::Angry:     return QColor(255,  80,  80);  // 红青筋
        case Tier::Love:      return QColor(255, 110, 160);  // 粉颊
        case Tier::Focus:     return QColor(150, 220, 255);  // 专注点
        case Tier::Work:      return QColor(255, 206, 120);  // 咖啡暖
        case Tier::Game:      return QColor( 96, 255, 176);  // 霓虹绿
        case Tier::Rest:      return QColor(186, 204, 255);  // 淡蓝 Zzz
        case Tier::Exercise:  return QColor(255, 214, 140);  // 汗/暖
        case Tier::Edm:       return QColor(255, 235,  80);  // 金色闪电⚡
    }
    return QColor(255, 255, 255);
}

const GenreTheme& theme(Genre g) {
    // 字段顺序:bg, dim, frame, bpm, specIntensity, specLow, specHigh
    static const GenreTheme themes[kGenreCount] = {
        // EDM 电音:冷暗底,烈频谱 + 低频 kick
        { QColor(  6,   8,  15), QColor( 20,  40,  60), QColor( 18,  26,  42),
          128.0f, 1.30f, 1.35f, 1.10f },
        // Ballad 抒情:暖暗底,缓频谱
        { QColor( 15,  10,  18), QColor( 50,  36,  46), QColor( 34,  24,  36),
          72.0f,  0.70f, 0.90f, 0.55f },
        // Pop 流行:中性暗底,中频
        { QColor( 10,  14,  26), QColor( 44,  52,  74), QColor( 26,  32,  50),
          100.0f, 1.00f, 1.00f, 1.00f },
    };
    return themes[static_cast<int>(g)];
}

namespace {
QColor mixC(const QColor& a, const QColor& b, float k) {
    if (k < 0.0f) k = 0.0f;
    if (k > 1.0f) k = 1.0f;
    return QColor(
        static_cast<int>(a.red()   + (b.red()   - a.red())   * k),
        static_cast<int>(a.green() + (b.green() - a.green()) * k),
        static_cast<int>(a.blue()  + (b.blue()  - a.blue())  * k));
}
// 同色相纯压暗(各通道等比),保住色相+饱和,只降明度 → 渐变不发灰发脏。
QColor mulC(const QColor& c, float k) {
    auto f = [k](int v) {
        int x = static_cast<int>(v * k);
        return x < 0 ? 0 : (x > 255 ? 255 : x);
    };
    return QColor(f(c.red()), f(c.green()), f(c.blue()));
}
}  // namespace

void moodGradient(const QColor& color, const QColor& accent,
                  QColor& top, QColor& bottom) {
    // 取主/辅中值作基准色相;top 压到偏亮(白像素仍能浮起),bottom 压深。
    const QColor mid = mixC(color, accent, 0.5f);
    top    = mulC(mid, 0.80f);
    bottom = mulC(mid, 0.15f);
}

}  // namespace Palette
