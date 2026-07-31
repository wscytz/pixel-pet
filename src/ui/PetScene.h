#pragma once

#include <QRectF>
#include <QPointF>

#include "ui/PetAnimator.h"   // FrameState
#include "ui/BeadGrid.h"
#include "audio/WebAudioSource.h"  // WebStatus

class QPainter;

// 像素宠物「场景视图」:纯渲染,只读 SceneInput,与主控(PetWidget)解耦。
// 画什么 = 一条分层管线(card 渐变 → 点阵带 → 发光脸 → HUD);
// 加一层 = 加一个 paintX + paint() 里一行;加状态/数据 = 扩 SceneInput/Palette。
namespace PetScene {

// 音浪地板样式:柱状(默认)/ 镜像(上下对称)/ 波形(连续折线)。视图层开关,PetWidget 持久化。
enum class EqStyle { Bars, Mirror, Wave };

// 几何:由窗口 rect 一次算出,各层共享(鼠标命中也用它,保证一致)。
struct SceneLayout {
    QRectF card;        // 内容卡(去掉阴影边距)
    QRectF band;        // 横向点阵带(氛围)的外框
    QRectF eq;          // 脸下音浪地板(发光像素柱均衡器)
    QRectF statusDot;   // 左上状态点(常显)
    QRectF closeBtn;    // 右上关闭按钮 ×(仅 hover 命中/绘制)
    QRectF minBtn;      // 右上最小化按钮 —(关闭左侧)
    int    cell = 0;    // 脸像素格大小(px)
    qreal  scale = 1.0; // 差速缩放因子(card<360px 时<1,各层几何次线性缩水让位给脸)
    float  ox = 0, oy = 0;  // 脸网格左上角(窗口坐标)
    int    gpx = 0, gpy = 0;  // 脸网格像素尺寸
};

// 视图输入:全部只读数据。st/face/spec 由主控每帧传入。
struct SceneInput {
    const FrameState& st;
    const BeadGrid&   face;
    const BeadGrid&   accent;    // 签名像素层(与 face 同尺寸)
    QColor            sigColor;  // 签名像素绘制色
    const float*      spec;     // 长度 specN 的频谱(0..1)
    int               specN;
    WebStatus         status;
    float             hoverAlpha;  // 控件淡入 0..1
    int               hoverBtn;    // 当前 hover 的按钮(0=close, -1=无)
    qint64            tMs;         // 真实时钟(点阵漂移等相位用)
    EqStyle           eqStyle;     // 音浪地板样式
};

SceneLayout layout(const QRectF& widgetRect);
void paint(QPainter& p, const QRectF& widgetRect, const SceneInput& in);

// 命中测试:0=关闭 ×,1=最小化 —,-1=无(鼠标命中与绘制共用,保证一致)
int buttonAt(const SceneLayout& L, const QPointF& p);

}  // namespace PetScene
