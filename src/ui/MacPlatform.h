#pragma once

#include <QtGlobal>   // Q_OS_DARWIN 宏(头文件被 .mm 先于 QWidget include 时也要能判平台)

class QWidget;

// macOS 专属:把窗口提到 NSStatusWindowLevel(状态栏级),
// 盖过普通窗口和浮动窗口——Qt 的 WindowStaysOnTopHint 在 macOS 只到 floating level。
// 其他平台 no-op(WindowStaysOnTopHint 已够)。
namespace MacPlatform {
#ifdef Q_OS_DARWIN
void makeTopMost(QWidget* w);
#else
inline void makeTopMost(QWidget*) {}
#endif
}
