#include <QApplication>
#include <QPixmap>
#include <QImage>
#include <QPainter>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QString>

#include <cstring>
#include <cstdlib>
#include <algorithm>

#include "ui/PetWidget.h"
#include "emotion/Emotion.h"
#include "emotion/EmotionMapper.h"

// 每档规范 valence/arousal 已并入 EmotionMapper::canonical(单一来源,手动模式共用)。

// 离屏渲染应用图标:海报式对角渐变 + 发光像素笑脸徽标。
// 徽标是手写小位图(非字体颜文字),缩小到 16px 仍认得出 → 分辨率无关的像素 logo。
static QImage renderIcon(int px) {
    QImage img(px, px, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    // brand 对角渐变(蓝 → 紫 → 品红)
    QLinearGradient g(0, 0, px, px);
    g.setColorAt(0.0, QColor(46, 120, 232));
    g.setColorAt(0.5, QColor(123, 77, 214));
    g.setColorAt(1.0, QColor(226, 64, 150));
    p.fillRect(img.rect(), g);
    // 暗角
    QRadialGradient vg(px * 0.5, px * 0.5, px * 0.72);
    vg.setColorAt(0.0, QColor(0, 0, 0, 0));
    vg.setColorAt(1.0, QColor(0, 0, 0, 80));
    p.fillRect(img.rect(), vg);

    static const char* E[] = {
        "...........",
        "..XX...XX..",
        "..XX...XX..",
        "...........",
        ".X.......X.",
        ".XXXXXXXXX.",
        "...........",
    };
    constexpr int rows = 7, cols = 11;
    const double cell = (px * 0.64) / cols;
    const double ox = (px - cell * cols) / 2.0;
    const double oy = (px - cell * rows) / 2.0;

    // 1) 低分辨率 alpha 掩码(连通实心笔画,避免逐像素缝隙)
    QImage mask(cols, rows, QImage::Format_ARGB32);
    mask.fill(Qt::transparent);
    for (int y = 0; y < rows; ++y)
        for (int x = 0; x < cols; ++x)
            if (E[y][x] == 'X') mask.setPixelColor(x, y, QColor(255, 255, 255));

    const QRectF base(ox, oy, cell * cols, cell * rows);
    // 2) 统一光晕:掩码平滑放大数层 → 柔和整体 bloom
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.setPen(Qt::NoPen);
    const struct { double e; float a; } G[] = { {0.95, 0.12f}, {0.55, 0.20f}, {0.26, 0.32f} };
    for (const auto& g2 : G) {
        p.setOpacity(g2.a);
        p.drawImage(base.adjusted(-g2.e * cell, -g2.e * cell, g2.e * cell, g2.e * cell), mask);
    }
    // 3) 芯:最近邻放大 → 硬边像素脸
    p.setOpacity(1.0);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.drawImage(base, mask);
    p.end();
    return img;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setOrganizationName("wscytz");     // QSettings 路径对齐 bundle id
    app.setApplicationName("PixelPet");

    // --dump <tier> <genre> <out.png>:渲染一帧静态图(自检用)
    if (argc >= 5 && std::strcmp(argv[1], "--dump") == 0) {
        const int ti = std::clamp(std::atoi(argv[2]), 0, kTierCount - 1);
        const int gi = std::clamp(std::atoi(argv[3]), 0, kGenreCount - 1);
        PetWidget w;
        Emotion e = EmotionMapper::canonical(static_cast<Tier>(ti));
        w.snapEmotion(e, static_cast<Genre>(gi));
        float demo[48];
        for (int i = 0; i < 48; ++i) {
            float v = 0.35f + 0.45f * std::sin(i * 0.45f) + 0.15f * std::sin(i * 1.3f);
            demo[i] = v < 0 ? 0 : (v > 1 ? 1 : v);
        }
        w.setSpectrum(demo, 48);
        w.show();
        if (argc >= 6) {   // 可选第 5 参:窗口尺寸(自检不同缩放下的脸)
            const int sz = std::clamp(std::atoi(argv[5]), 150, 520);
            w.resize(sz, sz);
        }
        w.repaint();
        QPixmap pix = w.grab();
        pix.save(QString::fromUtf8(argv[4]));
        std::fprintf(stderr, "=== tier %d genre %d ===\n%s", ti, gi, w.gridAscii().c_str());
        return 0;
    }

    // --sheet <out.png> [cellPx]:16 档脸谱图(4×4)—— README demo 用,复用 PetWidget 渲染
    if (argc >= 3 && std::strcmp(argv[1], "--sheet") == 0) {
        const int cell = (argc >= 4) ? std::clamp(std::atoi(argv[3]), 80, 520) : 200;
        constexpr int cols = 4, rows = 4;   // 16 档 = 10 情绪 + 6 活动
        QImage sheet(cell * cols, cell * rows, QImage::Format_ARGB32_Premultiplied);
        sheet.fill(Qt::transparent);
        QPainter p(&sheet);
        PetWidget w;
        w.resize(cell, cell);
        w.show();
        float demo[48];
        for (int i = 0; i < 48; ++i) {
            float v = 0.35f + 0.45f * std::sin(i * 0.45f) + 0.15f * std::sin(i * 1.3f);
            demo[i] = v < 0 ? 0 : (v > 1 ? 1 : v);
        }
        w.setSpectrum(demo, 48);
        for (int t = 0; t < kTierCount; ++t) {
            w.snapEmotion(EmotionMapper::canonical(static_cast<Tier>(t)), static_cast<Genre>(0));
            w.repaint();
            p.drawPixmap((t % cols) * cell, (t / cols) * cell, cell, cell, w.grab());
        }
        p.end();
        sheet.save(QString::fromUtf8(argv[2]));
        std::fprintf(stderr, "=== sheet: %d 档 %dx%d → %s ===\n", kTierCount, cell * cols, cell * rows, argv[2]);
        return 0;
    }

    // --icon <out.png> [size]:渲染单张图标 PNG(预览用)
    if (argc >= 3 && std::strcmp(argv[1], "--icon") == 0) {
        const int sz = (argc >= 4) ? std::max(16, std::atoi(argv[3])) : 1024;
        renderIcon(sz).save(QString::fromUtf8(argv[2]));
        return 0;
    }
    // --iconset <dir>:写一整套 .iconset 命名 PNG(供 iconutil 打 .icns)
    if (argc >= 3 && std::strcmp(argv[1], "--iconset") == 0) {
        const QString dir = QString::fromUtf8(argv[2]);
        const struct { const char* name; int px; } S[] = {
            {"icon_16x16.png", 16},     {"icon_16x16@2x.png", 32},
            {"icon_32x32.png", 32},     {"icon_32x32@2x.png", 64},
            {"icon_128x128.png", 128},  {"icon_128x128@2x.png", 256},
            {"icon_256x256.png", 256},  {"icon_256x256@2x.png", 512},
            {"icon_512x512.png", 512},  {"icon_512x512@2x.png", 1024},
        };
        for (const auto& s : S)
            renderIcon(s.px).save(dir + "/" + s.name);
        return 0;
    }

    PetWidget w;
    w.show();
    if (argc >= 2) w.loadFile(QString::fromLocal8Bit(argv[1]));  // 命令行直接传歌
    // 否则:右键「打开文件」或直接拖歌进来
    return app.exec();
}
