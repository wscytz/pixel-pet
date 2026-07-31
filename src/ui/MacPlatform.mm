#include "ui/MacPlatform.h"

#include <QWidget>

#import <AppKit/AppKit.h>

void MacPlatform::makeTopMost(QWidget* w) {
    if (!w) return;
    // QWidget::winId() 在 macOS 返回底层 NSView*;取其 NSWindow 提到最高实用层。
    NSView* view = (__bridge NSView*)reinterpret_cast<void*>(w->winId());
    NSWindow* win = [view window];
    if (!win) return;
    // 霸屏组合:
    //   NSPopUpMenuWindowLevel    盖过 status / floating / 普通窗口
    //   CanJoinAllSpaces          所有桌面空间都显示(切空间不消失)
    //   FullScreenAuxiliary       能在全屏 app 的空间上显示
    //   Stationary                不参与调度中心/Exposé 排列(常驻)
    //   IgnoresCycle              不进 Cmd+~ 窗口循环
    [win setHidesOnDeactivate:NO];   // 防 frameless/失活时被藏(配合去掉 Qt::Tool)
    [win setLevel:NSPopUpMenuWindowLevel];
    [win setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces
                         | NSWindowCollectionBehaviorStationary
                         | NSWindowCollectionBehaviorFullScreenAuxiliary
                         | NSWindowCollectionBehaviorIgnoresCycle];
}
