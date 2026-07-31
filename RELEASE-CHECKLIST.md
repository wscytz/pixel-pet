# PixelPet 发布前检查清单(v0.1.2 收官)

> **状态(2026-07-31)**:v0.1.1 已发布(含 16 档脸谱图 / CI dmg 31M 瘦身 / pp_test 单元测试 / 中英 README)。
> main 上 `e47f84d` 起的**代码健康修复未进 tag**(删 computeFromBins 死代码、节奏衰减回路、注释修正)。
> 本清单 = 晚上可视化测试 + 收尾发布 v0.1.2。逐项勾,发现问题就修。

## 一、可视化测试(放歌看宠物反应)

每项放 1-2 首代表曲,观察脸/动画/音浪/段落:

- [ ] **欢快(大调)**:脸 → Joyful/Healing,色暖,跳动积极
- [ ] **伤感(小调慢歌)**:脸 → Sad/Healing,色冷,动作放慢
- [ ] **dubstep/EDM**:脸 → Hype/Angry,音浪激烈,BPM 跟上
- [ ] **抒情流行(Am-F-C-G 关系调)**:脸不乱跳(调式模糊落 Healing/Sad,不误判欢快)
- [ ] **副歌段**:进副歌宠物更投入(section 感知)
- [ ] **音停**:宠物停下进 idle(**不再继续蹦** —— 验证 `e47f84d` 的 kickDensity 衰减修复)
- [ ] **人声播客**:脸中性(不乱跳音乐表情),Voice 场景判别对
- [ ] **游戏音效**:脸警戒(Noise 场景)

## 二、UI 观感

- [ ] **脸谱图** `assets/faces.png`:16 档清晰可辨?要不要加档名标签 / 调尺寸(用 `pixel-pet --sheet <out> [cellPx]` 重新生成)
- [ ] 不同窗口尺寸(150 迷你 ~ 520):差速缩放下脸仍可辨
- [ ] 透明度 / 锁定位置 / 音浪样式(柱状·镜像·波形)切换正常
- [ ] 拖拽移动 / 滚轮缩放 / 双击暂停 / 托盘恢复

## 三、功能

- [ ] **专注周报**:手动切活动档累积时长 → 右键「专注周报…」显示对
- [ ] **A/B 校准**:右键「校准情绪…」录欢快/伤感两首歌 → 个性化分界生效
- [ ] **设置页**:音源管理,自动判定当前驱动源

## 四、音源

- [ ] 本地文件(拖歌 / `pixel-pet <song>`)
- [ ] 浏览器扩展(网页版网易云 / B站 / YouTube,快捷键 Ctrl/Cmd+Shift+0)
- [ ] (有 Windows 机)系统音频 WASAPI loopback

## 五、收尾发布(v0.1.2)

1. [ ] 根据 1-4 的 feedback 调 UI / 修问题
2. [ ] bump 版本号:`CMakeLists.txt` 的 `MACOSX_BUNDLE_SHORT_VERSION_STRING` + `browser-extension/manifest.json` → `0.1.2`
3. [ ] 本地构建零警告 + `bash scripts/dump-faces.sh` + `./build/pp_test` 全过
4. [ ] commit + push(`--no-gpg-sign`;若 HTTPS push 超时被梯子拦 → `git push ssh://git@ssh.github.com:443/wscytz/pixel-pet.git main`)
5. [ ] 等 CI 双平台绿
6. [ ] 打 tag:`git -c tag.gpgsign=false tag -a v0.1.2 -m "PixelPet 0.1.2 ..."` + `git push ssh://...v0.1.2`(或 origin v0.1.2)
7. [ ] CI release job 出 draft → `gh release edit v0.1.2 --draft=false --latest --notes "..."`(中英 notes,参考 v0.1.1)
8. [ ] 确认 release:macOS dmg(~31M)+ Windows zip 挂上

## 已知限制(诚实,不必在结项前解)

- **backbeat 真实混音检测**:稠密混音分不开军鼓(每频带被占满),需 Goto 谐波抑制,留 backlog。backbeat 是诊断列、不接宠物行为。
- **Windows 真机 WASAPI**:CI 只证编得过,音频实跑要 Windows 机器测(第四节)。
- **mac 系统音频**:无(loopback 暂不做,网页扩展兜底)。

## 规矩提醒(执行时守)

- commit message 中性叙事,不点竞品/不说独家空白(narrative-principle)。
- 对外(README/release notes)只讲方向+特性+可行性,公司名(网易云/B站等)按需脱敏。
- tag 必须在 CI 绿之后打(留手测绿);bump≠tag。
- HTML 渲染仅在用户明说时跑。
