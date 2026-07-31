#pragma once

#include <QColor>
#include "emotion/Emotion.h"

// 颜色跟情绪档(每档独特色);流派只管底色 + 音浪风格 + 节拍基调。
namespace Palette {

// 情绪档主色 / 辅色(10 档,色相分散,独特辨识)
QColor tierColor(Tier t);
QColor tierAccent(Tier t);

// 该档签名像素(泪/脸颊/火花/青筋/汗/Zzz)的绘制色;无签名档返回白(不会被画到)。
QColor tierSignature(Tier t);

// 流派视觉主题:底色 + 音浪烈度/低高频 + 节拍
struct GenreTheme {
    QColor bg;
    QColor dim;
    QColor frame;
    float  bpm = 120.0f;
    float  specIntensity = 1.0f;
    float  specLow = 1.0f;
    float  specHigh = 1.0f;
};

const GenreTheme& theme(Genre g);

// 情绪渐变画布:由当前(已平滑 lerp 的)tier 主色+辅色派生竖向两 stop。
// top 保留色相但压到中暗(让白像素浮起),bottom 压到近黑。纯函数,逐帧调用廉价。
void moodGradient(const QColor& color, const QColor& accent,
                  QColor& top, QColor& bottom);

}  // namespace Palette
