#include "ui/PetScene.h"
#include "ui/Palette.h"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QPen>
#include <QColor>

#include <cmath>
#include <algorithm>

namespace PetScene {

namespace {
constexpr qreal SH = 10.0;     // 阴影边距
constexpr qreal RAD = 18.0;    // 卡圆角
constexpr int FACE_W = 48, FACE_H = 36;
constexpr qreal SIDE_MARGIN = 16.0;
constexpr qreal TOP_MARGIN = 34.0;   // 顶部留白(状态点 + 呼吸)
constexpr qreal EQ_ZONE = 60.0;      // 底部音浪地板高度

void paintCard(QPainter& p, const SceneLayout& L, const SceneInput& in) {
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    // 软投影
    for (int i = 5; i >= 1; --i) {
        p.setBrush(QColor(0, 0, 0, 6 * i));
        p.drawRoundedRect(L.card.adjusted(-i * 1.2, -i * 1.2, i * 1.2, i * 1.2), RAD + i, RAD + i);
    }
    // 情绪渐变画布
    QColor top, bot;
    Palette::moodGradient(in.st.color, in.st.accent, top, bot);
    QLinearGradient g(L.card.left() + L.card.width() * 0.3, L.card.top(),
                      L.card.left() + L.card.width() * 0.7, L.card.bottom());
    g.setColorAt(0.0, top);
    g.setColorAt(1.0, bot);
    p.setBrush(g);
    p.drawRoundedRect(L.card, RAD, RAD);
    // 暗角增纵深
    QRadialGradient vg(L.card.center(), L.card.width() * 0.75);
    vg.setColorAt(0.0, QColor(0, 0, 0, 0));
    vg.setColorAt(1.0, QColor(0, 0, 0, 70));
    p.setBrush(vg);
    p.drawRoundedRect(L.card, RAD, RAD);
    // 玻璃内高光边
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 38), 1.0));
    p.drawRoundedRect(L.card.adjusted(0.5, 0.5, -0.5, -0.5), RAD - 0.5, RAD - 0.5);
    p.setPen(Qt::NoPen);
}

void paintDotBand(QPainter& p, const SceneLayout& L, const SceneInput& in) {
    // 横向点阵带 = 活均衡器:列亮度跟 spec + beat,边缘淡出,慢漂移。纯像素硬边。
    p.setRenderHint(QPainter::Antialiasing, false);
    const qreal cx0 = L.card.left(), cw = L.card.width();
    const qreal fcy = L.oy + L.gpy * 0.5;
    const qreal halfH = L.band.height() * 0.5;
    const qreal pitch = 9.0 * L.scale, dot = 2.0;
    const float beat = in.st.beatPulse;
    const float drift = static_cast<float>(in.tMs) * 0.0006f;
    for (qreal y = L.band.top() + pitch * 0.5; y < L.band.bottom(); y += pitch) {
        const float hy = static_cast<float>((y - fcy) / halfH);
        const float envY = std::exp(-hy * hy * 2.2f);
        if (envY < 0.02f) continue;
        int col = 0;
        for (qreal x = cx0 + pitch * 0.5; x < L.card.right(); x += pitch, ++col) {
            const float hx = static_cast<float>((x - cx0) / cw);
            float envX = std::min(hx / 0.22f, (1.0f - hx) / 0.22f);
            if (envX < 0.0f) envX = 0.0f; else if (envX > 1.0f) envX = 1.0f;
            if (envX < 0.02f) continue;
            float s = 0.0f;
            if (in.specN > 0) {
                int si = static_cast<int>(hx * (in.specN - 1));
                if (si < 0) si = 0; else if (si >= in.specN) si = in.specN - 1;
                s = in.spec[si];
                if (s < 0.0f) s = 0.0f; else if (s > 1.0f) s = 1.0f;
            }
            const float driftv = 0.5f + 0.5f * std::sin(drift + col * 0.5f);
            float bright = (0.09f + 0.05f * driftv + 0.45f * s + 0.16f * beat) * envX * envY;
            if (bright < 0.03f) continue;
            if (bright > 1.0f) bright = 1.0f;
            p.fillRect(QRectF(x, y, dot, dot), QColor(255, 255, 255, static_cast<int>(bright * 255)));
        }
    }
}

void paintFaceGlow(QPainter& p, const SceneLayout& L, const SceneInput& in) {
    p.save();
    p.translate(0.0, -in.st.beatPulse * 2.0);   // 整脸随拍 bob

    const float b = in.st.beatPulse;
    const int W = in.face.W, H = in.face.H;
    const auto isLit = [&](int x, int y) { return in.face.v[y * W + x] != 0; };

    // halo:两层放大圆角方块叠 bloom(同 alpha 全脸共用,brush 设一次)
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    QColor h1 = in.st.accent; h1.setAlphaF(0.10f + 0.06f * b);
    p.setBrush(h1);
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
        if (!isLit(x, y)) continue;
        const qreal ccx = L.ox + x * L.cell + L.cell * 0.5;
        const qreal ccy = L.oy + y * L.cell + L.cell * 0.5;
        p.drawRoundedRect(QRectF(ccx - L.cell, ccy - L.cell, L.cell * 2, L.cell * 2), 3, 3);
    }
    QColor h2 = in.st.accent; h2.setAlphaF(0.16f + 0.08f * b);
    p.setBrush(h2);
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
        if (!isLit(x, y)) continue;
        const qreal ccx = L.ox + x * L.cell + L.cell * 0.5;
        const qreal ccy = L.oy + y * L.cell + L.cell * 0.5;
        p.drawRoundedRect(QRectF(ccx - L.cell * 0.72, ccy - L.cell * 0.72,
                                 L.cell * 1.44, L.cell * 1.44), 2, 2);
    }
    // core:crisp 近白像素(略染 tier 色),硬边
    p.setRenderHint(QPainter::Antialiasing, false);
    const QColor core(
        255 + static_cast<int>((in.st.color.red()   - 255) * 0.12f),
        255 + static_cast<int>((in.st.color.green() - 255) * 0.12f),
        255 + static_cast<int>((in.st.color.blue()  - 255) * 0.12f));
    p.setBrush(core);
    const qreal pad = L.cell * 0.12;
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
        if (!isLit(x, y)) continue;
        p.fillRect(QRectF(L.ox + x * L.cell + pad, L.oy + y * L.cell + pad,
                          L.cell - 2 * pad, L.cell - 2 * pad), core);
    }
    // 签名像素(泪/脸颊/火花/青筋/汗/Zzz):tier 签名色 + 一层光晕
    const QColor sig = in.sigColor;
    p.setRenderHint(QPainter::Antialiasing, true);
    QColor sh = sig; sh.setAlphaF(0.24f + 0.08f * b);
    p.setBrush(sh);
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
        if (!in.accent.v[y * W + x]) continue;
        const qreal ccx = L.ox + x * L.cell + L.cell * 0.5;
        const qreal ccy = L.oy + y * L.cell + L.cell * 0.5;
        p.drawRoundedRect(QRectF(ccx - L.cell * 0.85, ccy - L.cell * 0.85,
                                 L.cell * 1.7, L.cell * 1.7), 2, 2);
    }
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setBrush(sig);
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
        if (!in.accent.v[y * W + x]) continue;
        p.fillRect(QRectF(L.ox + x * L.cell + pad, L.oy + y * L.cell + pad,
                          L.cell - 2 * pad, L.cell - 2 * pad), sig);
    }
    p.restore();
}

void paintSpectrum(QPainter& p, const SceneLayout& L, const SceneInput& in) {
    // 脸下音浪地板:柱状/镜像/波形三态(共用频谱 + 踩拍 + 边缘淡出),把脸"托"起来。
    const QRectF eq = L.eq;
    if (eq.height() < 6) return;
    const int N = 28;
    const qreal step = eq.width() / N;
    const qreal barW = step * 0.6;
    const float beat = in.st.beatPulse;
    const float tMs = static_cast<float>(in.tMs);
    const QColor& a = in.st.accent;
    const QColor core(a.red()   + (255 - a.red())   / 3,
                      a.green() + (255 - a.green()) / 3,
                      a.blue()  + (255 - a.blue())  / 3);
    p.setPen(Qt::NoPen);

    // 共用:每列高度系数 hf[0..N)
    float hf[28];
    for (int i = 0; i < N; ++i) {
        const float t = (i + 0.5f) / N;
        float envX = std::min(t / 0.12f, (1.0f - t) / 0.12f);
        if (envX < 0.0f) envX = 0.0f; else if (envX > 1.0f) envX = 1.0f;
        float s = 0.0f;
        if (in.specN > 0) {
            int si = static_cast<int>(t * (in.specN - 1));
            if (si < 0) si = 0; else if (si >= in.specN) si = in.specN - 1;
            s = in.spec[si]; if (s < 0.0f) s = 0.0f; else if (s > 1.0f) s = 1.0f;
        }
        const float driftv = 0.5f + 0.5f * std::sin(tMs * 0.003f + i * 0.6f);
        // 频谱 s 定基底;踩拍 beat + 慢漂 driftv 权重加大,让中频稳定段也跟拍起伏 + 流动,
        // 不再「中间几根钉死不动」(真实音频中频能量天生平稳,原来 15%/8% 权重压不住)。
        float h = (0.10f + 0.90f * s) * (0.60f + 0.40f * beat) * (0.78f + 0.22f * driftv) * envX;
        if (h < 0.04f) h = 0.04f; else if (h > 1.0f) h = 1.0f;
        hf[i] = h;
    }

    if (in.eqStyle == EqStyle::Wave) {
        // 波形:底部填充面 + 顶部高亮折线(顺滑 antialias,与柱光晕同质感)
        QPolygonF poly;
        poly << QPointF(eq.left(), eq.bottom());
        for (int i = 0; i < N; ++i)
            poly << QPointF(eq.x() + (i + 0.5) * step, eq.bottom() - hf[i] * eq.height());
        poly << QPointF(eq.right(), eq.bottom());
        QColor fill = a; fill.setAlphaF(0.30f + 0.15f * beat);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setBrush(fill);
        p.drawPolygon(poly);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(core, 1.6));
        QPolygonF line;
        for (int i = 0; i < N; ++i)
            line << QPointF(eq.x() + (i + 0.5) * step, eq.bottom() - hf[i] * eq.height());
        p.drawPolyline(line);
        p.setPen(Qt::NoPen);
        return;
    }

    QColor base = a; base.setAlphaF(0.14);
    if (in.eqStyle == EqStyle::Mirror) {
        // 镜像:以中线为轴上下对称柱阵
        const qreal midY = eq.top() + eq.height() * 0.5;
        const qreal H = eq.height() * 0.5;
        p.fillRect(QRectF(eq.x(), midY - 0.5, eq.width(), 1.0), base);
        for (int i = 0; i < N; ++i) {
            const qreal bx = eq.x() + i * step + (step - barW) * 0.5;
            const qreal h = hf[i] * H;
            p.setRenderHint(QPainter::Antialiasing, true);
            QColor gl = a; gl.setAlphaF(0.16f + 0.10f * beat);
            p.setBrush(gl);
            p.drawRoundedRect(QRectF(bx - 1, midY - h - 1, barW + 2, 2 * h + 2), 2, 2);
            p.setRenderHint(QPainter::Antialiasing, false);
            p.setBrush(core);
            p.fillRect(QRectF(bx, midY - h, barW, 2 * h), core);
            p.fillRect(QRectF(bx, midY - h, barW, std::min<qreal>(2.0, h)), QColor(255, 255, 255, 200));
            p.fillRect(QRectF(bx, midY + h - std::min<qreal>(2.0, h), barW, std::min<qreal>(2.0, h)), QColor(255, 255, 255, 200));
        }
        return;
    }

    // 柱状(默认):柱从底向上
    p.fillRect(QRectF(eq.x(), eq.bottom() - 1, eq.width(), 1), base);
    for (int i = 0; i < N; ++i) {
        const qreal h = hf[i] * eq.height();
        const qreal bx = eq.x() + i * step + (step - barW) * 0.5;
        const qreal by = eq.bottom() - h;
        // 光晕
        p.setRenderHint(QPainter::Antialiasing, true);
        QColor gl = in.st.accent; gl.setAlphaF(0.16f + 0.10f * beat);
        p.setBrush(gl);
        p.drawRoundedRect(QRectF(bx - 1, by - 1, barW + 2, h + 2), 2, 2);
        // 芯(硬边)+ 顶部高亮 cap
        p.setRenderHint(QPainter::Antialiasing, false);
        p.fillRect(QRectF(bx, by, barW, h), core);
        p.fillRect(QRectF(bx, by, barW, std::min<qreal>(2.0, h)), QColor(255, 255, 255, 200));
    }
}

void paintHud(QPainter& p, const SceneLayout& L, const SceneInput& in) {
    const qreal sc = L.scale;   // HUD 装饰随尺寸差速(小窗 chip/符号/按钮一起缩)
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    // 状态点(常显,极小,左上;垫暗 chip 保证渐变上可读)
    const QRectF sd = L.statusDot;
    p.setBrush(QColor(0, 0, 0, 70));
    p.drawRoundedRect(sd.adjusted(-5 * sc, -5 * sc, 5 * sc, 5 * sc), 5 * sc, 5 * sc);
    QColor dc;
    switch (in.status) {
        case WebStatus::Connected: dc = QColor(86, 214, 120); break;
        case WebStatus::Error:     dc = QColor(240, 92, 92); break;
        default:                   dc = QColor(255, 255, 255, 90); break;
    }
    if (in.status == WebStatus::Connected) {
        QColor gw = dc; gw.setAlpha(70);
        p.setBrush(gw);
        p.drawEllipse(sd.adjusted(-3 * sc, -3 * sc, 3 * sc, 3 * sc));
    }
    p.setBrush(dc);
    p.drawEllipse(sd);
    // 右上按钮(× 关闭 / — 最小化):仅 hover 淡入,共用同一套磨砂方块
    if (in.hoverAlpha > 0.01f) {
        p.save();
        p.setOpacity(in.hoverAlpha);
        auto btn = [&](const QRectF& br, int id, bool isClose) {
            const bool h = (in.hoverBtn == id);
            p.setBrush(h ? QColor(255, 255, 255, 60) : QColor(255, 255, 255, 22));
            p.drawRoundedRect(br, 5 * sc, 5 * sc);
            p.setPen(QPen(h ? QColor(255, 255, 255, 235) : QColor(255, 255, 255, 150), 2.2));
            const QPointF c = br.center();
            const double k = 5.0 * sc;
            if (isClose) {
                p.drawLine(QPointF(c.x() - k, c.y() - k), QPointF(c.x() + k, c.y() + k));
                p.drawLine(QPointF(c.x() - k, c.y() + k), QPointF(c.x() + k, c.y() - k));
            } else {
                p.drawLine(QPointF(br.left() + k, c.y()), QPointF(br.right() - k, c.y()));
            }
            p.setPen(Qt::NoPen);
        };
        btn(L.closeBtn, 0, true);
        btn(L.minBtn,   1, false);
        p.restore();
    }
}

}  // namespace

SceneLayout layout(const QRectF& wr) {
    SceneLayout L;
    L.card = wr.adjusted(SH, SH, -SH, -SH);
    const qreal cw = L.card.width(), ch = L.card.height();
    // 差速缩放:card 较小时,边距/音浪/按钮/状态点次线性缩水(上限 1.0 → 大窗保持已批准观感),
    // 把比例让给脸 —— 小窗脸占比↑、音浪地板↓,cramped 尺寸下脸仍是一眼可辨的主角。
    // 下限 0.45:迷你窗(150~180)装饰也跟缩,别在 150 里塞 240 的按钮/地板。
    L.scale = std::clamp(ch / 360.0, 0.45, 1.0);
    const qreal s     = L.scale;
    const qreal sideM = SIDE_MARGIN * s;
    const qreal topM  = TOP_MARGIN  * s;
    const qreal eqZ   = EQ_ZONE     * s;
    const qreal midH = ch - topM - eqZ;            // 脸可用中部高度
    L.cell = static_cast<int>(std::min((cw - 2 * sideM) / FACE_W, midH * 0.92 / FACE_H));
    if (L.cell < 2) L.cell = 2;
    L.gpx = L.cell * FACE_W;
    L.gpy = L.cell * FACE_H;
    L.ox = L.card.x() + (cw - L.gpx) / 2.0;
    L.oy = L.card.y() + topM + (midH - L.gpy) / 2.0;   // 脸上移,底部让给音浪
    const qreal bandH = L.gpy + L.cell * 5;
    const qreal fcy = L.oy + L.gpy * 0.5;
    L.band = QRectF(L.card.x(), fcy - bandH * 0.5, cw, bandH);
    const qreal eqPadX = 24.0 * s;
    L.eq = QRectF(L.card.x() + eqPadX, L.card.bottom() - eqZ + 8.0 * s,
                  cw - 2 * eqPadX, eqZ - 18.0 * s);
    const qreal dot = 4.0 * s;
    const qreal cx = L.card.x() + 18.0 * s, cy = L.card.y() + 18.0 * s;
    L.statusDot = QRectF(cx - dot, cy - dot, dot * 2, dot * 2);
    const qreal bs = 22.0 * s, m = 10.0 * s, gap = 6.0 * s;
    L.closeBtn = QRectF(L.card.right() - m - bs, L.card.y() + m, bs, bs);
    L.minBtn   = QRectF(L.closeBtn.left() - gap - bs, L.card.y() + m, bs, bs);
    return L;
}

int buttonAt(const SceneLayout& L, const QPointF& p) {
    if (L.closeBtn.contains(p)) return 0;   // ×
    if (L.minBtn.contains(p))   return 1;   // —
    return -1;
}

void paint(QPainter& p, const QRectF& wr, const SceneInput& in) {
    const SceneLayout L = layout(wr);
    paintCard(p, L, in);
    // 前景(点阵带+脸)裁剪到圆角卡内,防光晕溢出
    p.save();
    QPainterPath clip;
    clip.addRoundedRect(L.card, RAD, RAD);
    p.setClipPath(clip);
    paintDotBand(p, L, in);
    paintFaceGlow(p, L, in);
    paintSpectrum(p, L, in);
    p.restore();
    paintHud(p, L, in);
}

}  // namespace PetScene
