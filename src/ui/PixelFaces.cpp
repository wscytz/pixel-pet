#include "ui/PixelFaces.h"

#include <cmath>

// 程序化像素脸(颜文字级):用 BeadGrid 图元手绘,不依赖字体(稳、可缩放、好维护)。
// 颜文字表现力三要素:① 眼睛形状(最重要部件)② 标志性嘴形(ω/益/∀/ᴥ)③ 手臂姿态 + 彩色配件。
// 脸走白通道(paintFace:眼/眉/嘴/手臂,纯形状),配件走第二颜色通道(paintAccent:
// 泪/音符/心/火花/汗/闪电…,视图里用 tier 签名色绘制)。blink>0.5 切闭眼帧。加一档 = 加一行 kSpec。
namespace PixelFaces {

namespace {
constexpr int GW = 48, GH = 36;
constexpr float PI = 3.14159265358979f;

// 眼(最重要部件):圆/笑/满足弧/闭/惊/爱心/星/怒/墨镜/点
enum { E_ROUND, E_HAPPY, E_HAPPYCLOSE, E_CLOSED, E_WIDEO, E_HEART,
       E_STAR, E_ANGRY, E_GLASSES, E_DOT };
// 嘴:笑/咧嘴/大笑∀/张O/平＿/瘪∩/波﹏/小ˍ/猫ω/咬牙益/吐舌ᴥ
enum { M_SMILE, M_GRIN, M_LAUGH, M_OPEN, M_FLAT, M_FROWN, M_WAVY,
       M_SMALL, M_CAT, M_GRIT, M_TONGUE };
// 眉
enum { B_NONE, B_ANGRY, B_RAISED, B_WORRY, B_FOCUS };
// 手臂/姿态(画在两翼,白通道)
enum { A_NONE, A_UP, A_FIST, A_RUN, A_SHRUG };   // A_RUN=跑动摆臂(运动)
// 签名配件(彩色第二通道)
enum { S_NONE, S_TEARS, S_CHEEKS, S_SPARKLE, S_VEIN, S_SWEAT, S_ZZZ,
       S_NOTES, S_HEARTS, S_EXCLAIM, S_STEAM, S_BOLT, S_SPEED };

struct Spec { int eye, mouth, brow, arms, sig; };
// 顺序严格对齐 Tier 枚举。无尺寸数组 + static_assert:漏行 / 漏 bump kTierCount 都成编译错。
const Spec kSpec[] = {
    // ── 情绪档(音频自动识别)──
    {E_HAPPY,      M_GRIN,  B_RAISED, A_NONE, S_CHEEKS },  // Joyful   ^▽^  +腮红
    {E_STAR,       M_LAUGH, B_RAISED, A_UP,   S_NOTES  },  // Hype     ★∀★  举手♪
    {E_HAPPYCLOSE, M_SMILE, B_NONE,   A_NONE, S_SPARKLE},  // Healing  ˘‿˘  ✦
    {E_ROUND,      M_SMILE, B_NONE,   A_NONE, S_NONE   },  // Calm     •‿•
    {E_ROUND,      M_FROWN, B_WORRY,  A_NONE, S_TEARS  },  // Sad      T_T   泪
    {E_ANGRY,      M_WAVY,  B_ANGRY,  A_NONE, S_SWEAT  },  // Agitated ＞﹏＜ 汗
    {E_WIDEO,      M_OPEN,  B_RAISED, A_NONE, S_EXCLAIM},  // Surprised ⊙_⊙  ❗
    {E_CLOSED,     M_SMALL, B_NONE,   A_NONE, S_ZZZ    },  // Sleepy   -_-  zZ
    {E_ANGRY,      M_GRIT,  B_ANGRY,  A_FIST, S_VEIN   },  // Angry    ◣益◢ 拳头╬
    {E_HEART,      M_CAT,   B_NONE,   A_NONE, S_HEARTS },  // Love     ♥ω♥  飘心
    // ── 活动档(手动模式专用;mapTier 永不返回)──
    {E_GLASSES,    M_FLAT,  B_NONE,   A_NONE, S_NONE   },  // Focus    ⌐■_■ 锁定
    {E_ROUND,      M_FLAT,  B_FOCUS,  A_NONE, S_STEAM  },  // Work     •_•  ☕热气
    {E_STAR,       M_TONGUE,B_NONE,   A_NONE, S_SPEED  },  // Game     ★ᴥ★ 耍帅+速度线(手在键盘,无臂)
    {E_HAPPYCLOSE, M_CAT,   B_NONE,   A_NONE, S_ZZZ    },  // Rest     ˘ω˘  zZ
    {E_ROUND,      M_OPEN,  B_ANGRY,  A_RUN,  S_SWEAT  },  // Exercise ◕▭◕ 跑臂💦 累喘
    {E_WIDEO,      M_LAUGH, B_RAISED, A_UP,   S_BOLT   },  // Edm      ⊙∀⊙  举手⚡ 电音嗨
};
static_assert(sizeof(kSpec) / sizeof(*kSpec) == kTierCount,
              "kSpec 行数必须等于 kTierCount(加 Tier 时在此加一行)");

constexpr int LX = 16, RX = 32, EY = 14, MX = 24, MY = 25, BY = 9;

// ── 眼睛 ──(side: -1 左 / +1 右;仅不对称眼型用)
void drawEye(BeadGrid& g, int cx, int style, int side) {
    switch (style) {
        case E_ROUND:   // • 圆眼(实心点;光晕自然柔化成圆,留白缺口在小尺寸像被咬)
            g.rect(cx - 1, EY - 1, 3, 3);
            break;
        case E_HAPPY:   // ^^ 折线笑眼
            g.line(cx - 2, EY + 1, cx, EY - 1);
            g.line(cx, EY - 1, cx + 2, EY + 1);
            break;
        case E_HAPPYCLOSE:  // ⌒ 满足闭眼弧(˘)
            g.arc(cx, EY, 2.4f, 0.15f * PI, 0.85f * PI);
            break;
        case E_CLOSED:  // – 平闭眼
            g.line(cx - 2, EY, cx + 2, EY);
            break;
        case E_WIDEO:   // ⊙ 空心大眼(惊)
            g.arc(cx, EY, 2.0f, 0, 2 * PI);
            break;
        case E_HEART:   // ♥ 爱心眼
            g.add(cx - 1, EY - 1); g.add(cx + 1, EY - 1);
            for (int x = cx - 2; x <= cx + 2; ++x) g.add(x, EY);
            g.add(cx - 1, EY + 1); g.add(cx, EY + 1); g.add(cx + 1, EY + 1);
            g.add(cx, EY + 2);
            break;
        case E_STAR:    // ★ 星眼(实心十字芯 + 四尖,不留洞)
            g.add(cx, EY); g.add(cx - 1, EY); g.add(cx + 1, EY);
            g.add(cx, EY - 1); g.add(cx, EY + 1);              // 实心芯
            g.add(cx, EY - 2); g.add(cx, EY + 2); g.add(cx - 2, EY); g.add(cx + 2, EY);  // 四尖
            g.add(cx - 1, EY - 1); g.add(cx + 1, EY + 1);     // 斜补更饱满
            break;
        case E_ANGRY:   // ＞＜ 怒/憋眼:朝鼻梁的折线(不对称)
            if (side < 0) { g.line(cx - 2, EY - 2, cx + 1, EY); g.line(cx + 1, EY, cx - 2, EY + 2); }
            else          { g.line(cx + 2, EY - 2, cx - 1, EY); g.line(cx - 1, EY, cx + 2, EY + 2); }
            break;
        case E_GLASSES: // ⌐■ 墨镜:实心镜片 + 上框(鼻梁在 renderFace 连)
            g.line(cx - 2, EY - 2, cx + 2, EY - 2);   // 上框
            g.rect(cx - 2, EY - 1, 5, 3);             // 实心镜片
            break;
        case E_DOT:     // ˙ 小点眼
            g.add(cx, EY); g.add(cx, EY + 1);
            break;
    }
}

// ── 眉 ──
void drawBrow(BeadGrid& g, int cx, int style, int side) {
    switch (style) {
        case B_ANGRY:   // 内低外高(怒)
            if (side < 0) g.line(cx - 2, BY - 1, cx + 2, BY + 1);
            else          g.line(cx - 2, BY + 1, cx + 2, BY - 1);
            break;
        case B_RAISED:  // ^ 抬眉
            g.line(cx - 2, BY, cx, BY - 1); g.line(cx, BY - 1, cx + 2, BY);
            break;
        case B_WORRY:   // 内高外低(忧)
            if (side < 0) g.line(cx - 2, BY + 1, cx + 2, BY - 1);
            else          g.line(cx - 2, BY - 1, cx + 2, BY + 1);
            break;
        case B_FOCUS:   // ˍˍ 平直严肃眉
            g.line(cx - 2, BY, cx + 2, BY);
            break;
        default: break;
    }
}

// ── 嘴 ──
void drawMouth(BeadGrid& g, int style) {
    switch (style) {
        case M_SMILE: g.arc(MX, MY, 5, PI, 2 * PI); break;              // ◡
        case M_FROWN: g.arc(MX, MY, 5, 0, PI); break;                   // ∩
        case M_GRIN:  g.arc(MX, MY, 5, PI, 2 * PI);
                      g.line(MX - 5, MY, MX + 5, MY); break;            // ▽ 咧嘴
        case M_LAUGH: // ∀ 大笑/大喊(倒三角张口)
            g.line(MX - 4, MY - 1, MX + 4, MY - 1);
            g.line(MX - 4, MY - 1, MX - 2, MY + 2);
            g.line(MX + 4, MY - 1, MX + 2, MY + 2);
            g.line(MX - 2, MY + 2, MX + 2, MY + 2);
            break;
        case M_OPEN:  // O 张口(放大空心;惊讶/喘气)
            g.line(MX - 2, MY - 1, MX + 2, MY - 1);
            g.line(MX - 2, MY + 2, MX + 2, MY + 2);
            g.add(MX - 2, MY); g.add(MX + 2, MY);
            g.add(MX - 2, MY + 1); g.add(MX + 2, MY + 1);
            break;
        case M_FLAT:  g.line(MX - 3, MY, MX + 3, MY); break;            // ＿
        case M_WAVY:  g.arc(MX - 3, MY - 1, 3, PI, 2 * PI);
                      g.arc(MX + 3, MY - 1, 3, PI, 2 * PI); break;      // ﹏
        case M_SMALL: g.line(MX - 1, MY, MX + 1, MY); break;            // ˍ
        case M_CAT:   // ω 猫嘴(两相邻小杯)
            g.arc(MX - 2, MY - 1, 2, PI, 2 * PI);
            g.arc(MX + 2, MY - 1, 2, PI, 2 * PI);
            break;
        case M_GRIT:  // 益 咬牙(框 + 竖齿)
            g.line(MX - 3, MY - 1, MX + 3, MY - 1);
            g.line(MX - 3, MY + 1, MX + 3, MY + 1);
            g.line(MX - 1, MY - 1, MX - 1, MY + 1);
            g.line(MX + 1, MY - 1, MX + 1, MY + 1);
            break;
        case M_TONGUE:  // ᴥ 吐舌(连贯笑U + 垂舌)
            g.line(MX - 4, MY - 1, MX - 2, MY + 1);
            g.line(MX - 2, MY + 1, MX + 2, MY + 1);
            g.line(MX + 2, MY + 1, MX + 4, MY - 1);
            g.add(MX - 1, MY + 2); g.add(MX, MY + 2); g.add(MX + 1, MY + 2);
            g.add(MX, MY + 3);
            break;
    }
}

// ── 手臂/姿态 ──(白通道,画在两翼 x<10 / x>38)
void drawArms(BeadGrid& g, int style) {
    switch (style) {
        case A_UP:    // ╰(*▽*)╯ 贴脸两侧竖臂 + 顶手 + 贴脸脚
            g.line(7, 18, 7, 27); g.line(7, 27, 11, 28);
            g.add(6, 17); g.add(7, 16); g.add(8, 17);
            g.line(41, 18, 41, 27); g.line(41, 27, 37, 28);
            g.add(40, 17); g.add(41, 16); g.add(42, 17);
            break;
        case A_FIST:  // (ง •̀_•́)ง 握拳朝内
            g.rect(4, 22, 3, 3); g.line(7, 23, 11, 21);
            g.rect(41, 22, 3, 3); g.line(41, 23, 37, 21);
            break;
        case A_RUN:   // 跑动摆臂:右臂屈肘前摆 + 拳,左臂屈肘后摆
            g.line(39, 24, 43, 22); g.line(43, 22, 43, 18);
            g.add(42, 17); g.add(43, 16); g.add(44, 17);
            g.line(9, 24, 5, 26); g.line(5, 26, 5, 29);
            break;
        case A_SHRUG: // ╮(╯_╰)╭ 摊手
            g.line(10, 22, 4, 20); g.add(3, 19);
            g.line(38, 22, 44, 20); g.add(45, 19);
            break;
        default: break;
    }
}

void renderFace(Tier t, bool closed, BeadGrid& g) {
    g.clear();
    const Spec& s = kSpec[static_cast<int>(t)];
    const int eye = closed ? E_CLOSED : s.eye;
    // 自带表情的眼型(^/爱心/星/满足弧/墨镜/闭)再叠眉会撞 → 压掉眉
    const bool suppress = (eye == E_HAPPY || eye == E_HEART || eye == E_STAR ||
                           eye == E_HAPPYCLOSE || eye == E_GLASSES || eye == E_CLOSED);
    const int brow = suppress ? B_NONE : s.brow;
    drawArms(g, s.arms);
    drawBrow(g, LX, brow, -1);
    drawBrow(g, RX, brow, +1);
    drawEye(g, LX, eye, -1);
    drawEye(g, RX, eye, +1);
    if (eye == E_GLASSES) g.line(LX + 2, EY - 2, RX - 2, EY - 2);   // 墨镜鼻梁
    drawMouth(g, s.mouth);
}

// ── 签名配件辅助(彩色通道)──
void drawZ(BeadGrid& g, int x, int y) {
    g.line(x, y, x + 2, y);
    g.line(x + 2, y, x, y + 2);
    g.line(x, y + 2, x + 2, y + 2);
}
void drawHeart(BeadGrid& g, int x, int y) {   // 3×3 小心,顶行左右点 + 中行满 + 底尖
    g.add(x, y); g.add(x + 2, y);
    g.add(x, y + 1); g.add(x + 1, y + 1); g.add(x + 2, y + 1);
    g.add(x + 1, y + 2);
}
void drawSpark(BeadGrid& g, int x, int y) {   // ✦ 四芒小星
    g.add(x, y); g.add(x, y - 1); g.add(x, y + 1); g.add(x - 1, y); g.add(x + 1, y);
}
void drawNote(BeadGrid& g, int x, int y) {    // ♪ 音符(符头 + 符干 + 旗)
    g.disc(x, y, 1.0f);
    g.line(x + 1, y, x + 1, y - 4);
    g.line(x + 1, y - 4, x + 3, y - 3);
}

void renderAccent(Tier t, BeadGrid& g) {
    g.clear();
    switch (kSpec[static_cast<int>(t)].sig) {
        case S_TEARS:   // 双眼泪柱 + 滴
            for (int cx : {LX, RX}) {
                for (int yy = EY + 3; yy <= EY + 9; ++yy) g.add(cx, yy);
                g.add(cx, EY + 10);
            }
            break;
        case S_CHEEKS:  // 腮红块
            g.rect(10, 19, 3, 2); g.rect(35, 19, 3, 2);
            break;
        case S_SPARKLE: // ✦ 柔光点
            drawSpark(g, 40, 7); drawSpark(g, 7, 6); drawSpark(g, 42, 12);
            break;
        case S_VEIN:    // ╬ 青筋
            g.line(39, 6, 39, 10); g.line(41, 6, 41, 10);
            g.line(38, 7, 42, 7);  g.line(38, 9, 42, 9);
            break;
        case S_SWEAT:   // 汗滴(尖顶 + 圆底,别收成菱形)
            g.add(40, 10);
            g.add(40, 11);
            g.add(39, 12); g.add(40, 12); g.add(41, 12);
            g.add(39, 13); g.add(40, 13); g.add(41, 13);
            break;
        case S_ZZZ:     // Zzz(大 + 小)
            drawZ(g, 36, 7); drawZ(g, 41, 3);
            break;
        case S_NOTES:   // ♪♫ 漂浮音符
            drawNote(g, 7, 11); drawNote(g, 40, 9);
            break;
        case S_HEARTS:  // ♡ 飘心
            drawHeart(g, 6, 5); drawHeart(g, 40, 4); drawHeart(g, 41, 10);
            break;
        case S_EXCLAIM: // ❗ 惊叹
            g.line(41, 4, 41, 9); g.add(41, 11);
            break;
        case S_STEAM:   // ☕ 杯子(右下)+ 上升热气
            g.line(39, 24, 39, 27); g.line(43, 24, 43, 27); g.line(39, 27, 43, 27);  // 杯身
            g.add(44, 25); g.add(44, 26);                                            // 把手
            g.add(40, 22); g.add(41, 21); g.add(40, 20); g.add(41, 19);             // 热气
            g.add(42, 22); g.add(43, 21); g.add(42, 20);
            break;
        case S_BOLT:    // ⚡ 闪电
            g.line(41, 4, 38, 8); g.line(38, 8, 41, 8); g.line(41, 8, 38, 13);
            break;
        case S_SPEED:   // 速度线(两侧水平短线)
            g.line(2, 12, 7, 12); g.line(3, 16, 7, 16); g.line(2, 20, 6, 20);
            g.line(41, 12, 46, 12); g.line(41, 16, 45, 16); g.line(42, 20, 46, 20);
            break;
        default: break;
    }
}

struct Cache {
    BeadGrid face[kTierCount * 2];
    bool fr[kTierCount * 2] = {};
    BeadGrid acc[kTierCount];
    bool ar[kTierCount] = {};
    Cache() {
        for (auto& g : face) g = BeadGrid(GW, GH);
        for (auto& g : acc) g = BeadGrid(GW, GH);
    }
};
Cache& cache() { static Cache c; return c; }

void copyGrid(const BeadGrid& src, BeadGrid& dst) {
    const int n = std::min(src.W * src.H, dst.W * dst.H);
    for (int i = 0; i < n; ++i) dst.v[i] = src.v[i];
}

}  // namespace

void paintFace(Tier t, float blink, BeadGrid& g) {
    Cache& c = cache();
    const bool closed = blink > 0.5f;
    const int idx = static_cast<int>(t) * 2 + (closed ? 1 : 0);
    if (!c.fr[idx]) { renderFace(t, closed, c.face[idx]); c.fr[idx] = true; }
    copyGrid(c.face[idx], g);
}

void paintAccent(Tier t, float /*blink*/, BeadGrid& g) {
    Cache& c = cache();
    const int ti = static_cast<int>(t);
    if (!c.ar[ti]) { renderAccent(t, c.acc[ti]); c.ar[ti] = true; }
    copyGrid(c.acc[ti], g);
}

}  // namespace PixelFaces
