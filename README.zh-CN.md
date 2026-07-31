[English](README.md) | 简体中文

# PixelPet

[![CI](https://github.com/wscytz/pixel-pet/actions/workflows/build.yml/badge.svg)](https://github.com/wscytz/pixel-pet/actions/workflows/build.yml)
[![License: GPL v2](https://img.shields.io/badge/License-GPL_v2-blue.svg)](LICENSE)
![platform](https://img.shields.io/badge/platform-macOS%20%7C%20Windows-lightgrey)
![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C)
![Qt 6](https://img.shields.io/badge/Qt-6-green)
![emotion](https://img.shields.io/badge/emotion-pure%20algorithm%20(no%20ML)-ff69b4)

一个常驻置顶的小桌宠,把你正在听的音乐变成会动的**像素颜文字脸** —— 欢快的歌让它微笑蹦跳,伤感的慢歌让它垂头放慢,dubstep 露出獠牙。纯算法情绪识别,**不用任何 ML 模型**。

## 它做什么

音频播放时,桌宠持续分析信号,映射到二维 **valence–arousal**(效价-唤醒)情绪空间(环状模型),再驱动程序化绘制的 LED 风格颜文字脸、配色和动作。

这张脸不是字体字形 —— 是用 `QPainter` 手绘的像素画(眼睛 / 眉毛 / 嘴巴 / 手臂,以及眼泪、爱心、闪电等签名配饰),任意尺寸都清晰锐利。

共 **16 档表情**:
- **10 种自动识别情绪**:欢快、兴奋、治愈、平静、伤感、躁动、惊讶、困倦、愤怒、恋爱
- **6 种手动活动模式**:专注、工作、游戏、休息、运动、电音 —— 锁定脸和配色,音浪地板仍随音频起伏

### 脸之外

- **段落感知** —— 用 chroma 自相似度和能量识别前奏 / 主歌 / 副歌 / 桥 / 尾奏;进入副歌时桌宠更投入。
- **节奏感** —— 底鼓密度驱动跳动,四拍底鼓比稀疏的抒情歌跳得更欢;反拍与切分同步跟踪。
- **情绪轨迹** —— 追踪 valence/arousal 随时间的漂移(升华 / 失落 / 蓄势 / 放松),而非逐帧反应。
- **个性化校准** —— A/B 锚定一首"欢快"和一首"伤感"的歌,桌宠适配你的大小调分界线(伤不伤感是主观的)。
- **专注统计** —— 各手动活动模式的时长累计成周报,本地存储。
- **音源面板** —— 设置页自动判定当前哪个音源在喂桌宠,可分别开关。

## 音源

| 音源 | 方式 | 说明 |
|---|---|---|
| 本地文件 | 把音乐文件拖到窗口,或 `pixel-pet <歌曲>` | 离线,适合单曲测试 |
| 浏览器标签页 | 配套扩展(`browser-extension/`)捕获当前标签页,经 WebSocket 推实时频谱 | 任意流媒体站点可用;快捷键 **Ctrl/Cmd+Shift+0** 开关捕获 |
| 系统音频 | **仅 Windows** —— WASAPI loopback 捕获默认输出设备 | 任何桌面 app 的声音都能喂桌宠,无需浏览器 |

三个音源共用同一条分析链路,情绪映射处处一致。

## 外观与操作

- **差速缩放** —— 窗口越小,脸占比越大,音浪 / 边距 / 按钮同步收窄,迷你尺寸下脸仍一眼可辨。
- 尺寸 **150px 迷你** 到 520px;中心锚定,位置和尺寸自动记忆。
- 无边框常驻置顶窗口;拖拽移动,滚轮 / 双指捏合缩放,悬停露出关闭 / 最小化。
- 可调:窗口透明度(40–100%)、锁定位置、音浪地板样式(**柱状 / 镜像 / 波形**)。
- 右键菜单统揽所有功能;最小化隐藏到托盘 / 菜单栏。

## 构建

见 [BUILD.md](BUILD.md) —— macOS(Homebrew Qt)和 Windows(MSVC + Qt6)都支持;GitHub Actions 每次 push 在双平台构建并打包。

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## 许可

[GPL-2.0](LICENSE)
