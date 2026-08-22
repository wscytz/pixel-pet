# Changelog

## 1.0.0 (2026-08-22)

首个稳定版。0.1.x 五轮迭代(16 档脸谱 / CI 双平台产物 / 单元测试 / 中英 README / WASAPI 自动启动 / 静音进 idle)后定档。

### 稳定版验证证据(2026-08-22)

- **测试**:pp_test 纯算法单测 12 断言全过(FFT / EmotionMapper / calibrateFromMs 分界有序)。
- **干净构建**:全新 build 目录 Release 配置,主程序 + pp_test 编译零警告(本地 homebrew Qt 链接器 6 条 deployment-target 提示为环境性,CI 用 aqt 6.11.1 独立 Qt 不受影响)。
- **审核**:Qt 界面 `new` 均为父子树托管(无泄漏);Windows WASAPI 采集线程独立 + atomic stop flag;-Wall -Wextra -Wpedantic -Werror=switch 严格档;CI 双平台(macOS dmg + Windows exe+DLL)每次 push 把关。
- **发布物**:推送 v1.0.0 tag 后 CI 自动建 GitHub Release 草稿(挂 dmg + windows zip)。

### 已知约束

- 桌宠音频情绪为纯算法(无 ML),调式模糊曲可能落 Healing/Sad——按设计非 bug。
- 听感类验收(放歌看反应,RELEASE-CHECKLIST 第一节)属人工项,随日常使用回归。

## 0.1.5 (2026-08-03)

unicode 本地文件路径、静音可闻门控进 idle、Windows 自动 WASAPI。
