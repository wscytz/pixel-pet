# PixelPet 浏览器插件

抓取 Chrome/Edge **当前标签页**的音频(任意网页播放站点),实时算频谱+RMS,通过 WebSocket 推给 PixelPet 桌宠。

## 架构(MV3,Chrome 116+)

```
popup 点"开始" → background(service worker)拿 getMediaStreamId
              → offscreen document 用 getUserMedia 消费流
              → AnalyserNode 出频谱(128 bins)+ RMS
              → WebSocket ws://127.0.0.1:17632 → C++ 桌宠
```

service worker 无 DOM 不能 `getUserMedia`,所以音频消费必须在 **offscreen document**(MV3 硬要求)。

## 装载

1. Chrome 打开 `chrome://extensions`,开"开发者模式"。
2. "加载已解压的扩展程序",选 `browser-extension/` 目录。
3. 打开要抓的网页,播放音乐。
4. 点扩展图标 → "开始抓当前标签",或按快捷键 `Ctrl+Shift+0`(mac `Command+Shift+0`)toggle 开始/停止。
5. PixelPet 桌宠在跑的话,会实时接收频谱并随音乐动。

## 数据契约

每 ~33ms 一条 JSON:
```json
{ "spectrum": [128 × 0..255], "rms": 0.0..1.0, "title": "歌名 - 歌手" }
```
- `spectrum`:AnalyserNode `getByteFrequencyData`,fftSize=256 → 128 bins
- `rms`:时域 RMS
- `title`:标签页标题(透传,备用)

ws 断开会自动每 2s 重连。

## 注意

- 只抓**主动选择的标签页**,不全局监听(隐私)。
- 桌面音乐客户端等浏览器外的播放器抓不到:Windows 上开桌宠右键「系统音频」(WASAPI loopback)抓系统输出即可;mac 无系统监听,走"打开文件/拖入"回退。
- 只 Chromium 系(Chrome/Edge);Firefox 的 tabCapture API 不同,未适配。
