# 构建 PixelPet

情绪识别纯算法(无 ML);仅 macOS 的窗口置顶 / 托盘用少量 AppKit(`src/ui/MacPlatform.mm`)。
逻辑同一份代码 macOS / Windows 都能编。

依赖:**Qt 6**(Widgets + Multimedia + WebSockets)+ **CMake ≥ 3.21** + C++17 编译器。

## macOS

```sh
brew install cmake qt
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# 跑(开发)
./build/pixel-pet.app/Contents/MacOS/pixel-pet

# 打包成可分发 .app/.dmg
# 注意:脚本会复制到 dist/ 再 macdeployqt —— 别直接在 build/pixel-pet.app 上跑
# macdeployqt,它会往 app 里复制 frameworks + 改 rpath,之后重 link 会双 Qt 加载崩。
bash scripts/package-mac.sh
# 产物:dist/PixelPet-<版本>.dmg + dist/PixelPet.app
```

## Windows

装 Visual Studio(含 CMake)+ Qt6(Widgets + Multimedia,MSVC 套件)。然后:

```bat
:: 改脚本里的 QT_PREFIX 指向你的 Qt,再跑:
scripts\build-windows.bat
```

或手动:
```bat
set CMAKE_PREFIX_PATH=C:\Qt\6.9.0\msvc2022_64
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
windeployqt --release build\Release\pixel-pet.exe
```
`windeployqt` 把 Qt DLL / platforms 插件打进 `build\Release\`,整个目录可分发。

## 运行

- **输入音频**(任选其一):
  - 命令行传歌:`pixel-pet 路径/歌.mp3`,或把音乐文件**拖到窗口**
  - **网页音源**:装 `browser-extension/`(浏览器扩展,tabCapture → WebSocket),浏览器放歌宠物实时反应;左上角小圆点是连接状态(绿=已连)
- **悬浮窗控件**:鼠标移入右上角 → `—` 最小化到菜单栏托盘 / `×` 退出
- **右键菜单**:设置… / 打开文件 / 暂停继续 / **模式**(自动·跟随音乐 ÷ 专注·工作·游戏·休息·运动·电音 手动切换)/ **大小**(迷你·小·中·大)/ **透明度**(40–100%)/ **音浪**(柱状·镜像·波形)/ **锁定位置** / 退出
- **设置**:右键「设置…」打开音源管理页(本地文件 / 网页扩展 / 系统音频,各带介绍 + 状态 + 手动开关;顶部「当前驱动」自动判定现在哪个源在喂宠物)
- **系统音频(仅 Windows)**:右键菜单「系统音频」开 WASAPI loopback,抓系统默认输出——桌面音乐客户端等任何 app 的声音都能喂宠物(mac 没有该能力,网页扩展兜底)
- **缩放**:`⌘`/`Ctrl` + 滚轮,或触控板双指捏合(中心锚定;尺寸/位置自动记忆)
- **差速缩放**:窗口越小,脸占比自动放大、音浪/边距/按钮同步收窄,小尺寸下脸仍是一眼可辨的主角(大窗观感不变)
- **托盘**:最小化后点菜单栏的像素脸图标 → 显示/隐藏;点 Dock 图标也会唤起
- **双击**暂停/继续,**ESC** 退出
- 无音频时:模拟轮播情绪 + 流派;表情共 **16 档**(10 情绪 + 6 活动:专注·工作·游戏·休息·运动·电音)

## 跨端 CI

`.github/workflows/build.yml`(GitHub Actions)在 **macOS + Windows(MSVC)** 各跑一遍编译 + 打包:
- Windows job 专门验证 `SystemAudioSource_win.cpp` 的 **WASAPI loopback** 在 MSVC 下能编能链接(`ole32`/`mmdevapi`)
- 两平台各出可分发产物(dmg / exe+Qt DLL),`windeployqt` / `macdeployqt` 自动打依赖

## 跨端注意

- 音频解码用 `QAudioDecoder`(Qt6.5+ 回归):macOS 走 AVFoundation,Windows 走 MediaFoundation——mp3/m4a/wav/flac 通用。
- `FramelessWindowHint | WindowStaysOnTopHint` 两平台都支持。
- 窗口拖动 / 拖入文件 / 右键菜单:Qt 跨平台事件,无差异。
- 系统音频(WASAPI)仅在 Windows 编译(`#ifdef Q_OS_WIN` + CMake `if(WIN32)`);mac 上菜单项隐藏、`start()` 返回 false。
