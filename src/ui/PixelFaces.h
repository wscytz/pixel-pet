#pragma once

#include "ui/BeadGrid.h"
#include "emotion/Emotion.h"

// 颜表情像素图:程序化用 BeadGrid 图元(眼/眉/嘴/手臂/配件)手绘,不依赖字体
// (稳、可缩放、好维护;加一档 = 在 PixelFaces.cpp 的 kSpec 加一行)。网格 48×36。
// 脸走白通道 paintFace;签名配件(泪/音符/心/汗/闪电…)走第二通道 paintAccent,
// 视图里用该档签名色绘制。blink>0.5 切闭眼帧。
namespace PixelFaces {
void paintFace(Tier t, float blink, BeadGrid& g);     // 主脸(白通道:眼/眉/嘴/手臂)
void paintAccent(Tier t, float blink, BeadGrid& g);   // 签名配件(泪/脸颊/音符/心/汗/Zzz…)
}
