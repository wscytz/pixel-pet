#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>
#include <vector>

// 像素网格缓冲。尺寸运行时给定(主颜文字区用 48×36,够放下眼/嘴/手臂/配件)。
// 各绘制方写入亮度,PetWidget 统一遍历渲染成方块。
struct BeadGrid {
    int W = 0, H = 0;
    std::vector<uint8_t> v;

    BeadGrid() = default;
    BeadGrid(int w, int h) : W(w), H(h), v(static_cast<size_t>(w) * h, 0) {}

    void clear() { std::fill(v.begin(), v.end(), 0); }

    void add(int x, int y, uint8_t b = 255) {
        if (x < 0 || x >= W || y < 0 || y >= H) return;
        int i = y * W + x;
        int n = static_cast<int>(v[i]) + b;
        v[i] = n > 255 ? 255 : static_cast<uint8_t>(n);
    }

    void addF(float x, float y, uint8_t b = 255) {
        add(static_cast<int>(std::round(x)), static_cast<int>(std::round(y)), b);
    }

    void line(int x0, int y0, int x1, int y1, uint8_t b = 255) {
        int dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);
        int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
        int err = dx - dy;
        for (;;) {
            add(x0, y0, b);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x0 += sx; }
            if (e2 < dx)  { err += dx; y0 += sy; }
        }
    }

    void rect(int x, int y, int w, int h, uint8_t b = 255) {
        for (int j = 0; j < h; ++j)
            for (int i = 0; i < w; ++i)
                add(x + i, y + j, b);
    }

    void arc(float cx, float cy, float r, float a0, float a1, uint8_t b = 255) {
        float span = std::abs(a1 - a0);
        int steps = std::max(4, static_cast<int>(r * span * 1.5f));
        for (int i = 0; i <= steps; ++i) {
            float a = a0 + (a1 - a0) * (i / static_cast<float>(steps));
            addF(cx + r * std::cos(a), cy - r * std::sin(a), b);
        }
    }

    void disc(float cx, float cy, float r, uint8_t b = 255) {
        int r0 = static_cast<int>(std::ceil(r));
        for (int dy = -r0; dy <= r0; ++dy)
            for (int dx = -r0; dx <= r0; ++dx)
                if (dx * dx + dy * dy <= r * r)
                    add(static_cast<int>(cx) + dx, static_cast<int>(cy) + dy, b);
    }
};
