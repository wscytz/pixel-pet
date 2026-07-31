#!/usr/bin/env bash
# macOS 打包:复制 .app → macdeployqt 打 Qt 依赖 → 瘦身(删桌面用不到的虚键/QML)→ ad-hoc 签名 → .dmg。
# 产物 = 完整陪伴件:app + 浏览器扩展 + 安装说明(扩展是网页音频的另一半产品)。
# ★ 关键:在 dist-stage 副本上打,绝不在 build/pixel-pet.app 上跑 macdeployqt
#   (它会往 app 里复制 frameworks + 改 rpath;之后改代码重 link 会双 Qt 加载崩,exit 134)。
set -euo pipefail
cd "$(dirname "$0")/.."

APP=build/pixel-pet.app
[ -d "$APP" ] || { echo "先构建:cmake --build build -j"; exit 1; }

# Qt bin(macdeployqt 所在)。按序探测:显式 QT_BIN → 环境 QTDIR → PATH 上的
# macdeployqt/qmake6 → aqtinstall($HOME/Qt/<ver>/<arch>/bin)→ Homebrew。
find_macdeployqt() {
    if [ -n "${QT_BIN:-}" ] && [ -x "$QT_BIN/macdeployqt" ]; then echo "$QT_BIN"; return; fi
    if [ -n "${QTDIR:-}" ] && [ -x "$QTDIR/bin/macdeployqt" ]; then echo "$QTDIR/bin"; return; fi
    local q; q="$(command -v macdeployqt 2>/dev/null || true)"
    [ -n "$q" ] && { echo "$(dirname "$q")"; return; }
    q="$(command -v qmake6 2>/dev/null || true)"
    [ -n "$q" ] && { echo "$(dirname "$q")"; return; }
    for d in "$HOME"/Qt/*/*/bin; do [ -x "$d/macdeployqt" ] && { echo "$d"; return; }; done
    [ -x /opt/homebrew/opt/qt/bin/macdeployqt ] && { echo /opt/homebrew/opt/qt/bin; return; }
    [ -x /usr/local/opt/qt/bin/macdeployqt ] && { echo /usr/local/opt/qt/bin; return; }
    echo ""
}
QT_BIN="$(find_macdeployqt)"
[ -n "$QT_BIN" ] || { echo "找不到 macdeployqt(装 Qt,或设 QT_BIN/QTDIR)"; exit 1; }

VER="$(/usr/libexec/PlistBuddy -c 'Print CFBundleShortVersionString' "$APP/Contents/Info.plist" 2>/dev/null || echo 0.1.0)"
rm -rf dist dist-stage
mkdir -p dist dist-stage
DST="dist-stage/PixelPet.app"
cp -R "$APP" "$DST"

echo "=== macdeployqt(Qt 依赖入 app)==="
"$QT_BIN/macdeployqt" "$DST" -verbose=1

echo "=== 瘦身:删桌面用不到的 Qt 部分 ==="
# 虚键插件(platforminputcontexts)+ 它连带拉入的 QML 全套 / QtOpenGL。
# 桌面 app 无触屏不用虚键;正确扫描(框架内部二进制,非目录)确认:除虚键插件外
# 无任何外部二进制引用 QML/QtOpenGL,它们只在自己族内互引。
# ⚠ QtDBus 不能删 —— QtGui.framework 直接 @rpath 链它(macOS 剪贴板服务),删了
#   dyld 找不到 → exit 134。QtConcurrent 同理,multimedia 后端要用。
rm -rf "$DST/Contents/PlugIns/platforminputcontexts"
rm -rf "$DST/Contents/Frameworks/QtQuick.framework" \
       "$DST/Contents/Frameworks/QtQml.framework" \
       "$DST/Contents/Frameworks/QtQmlModels.framework" \
       "$DST/Contents/Frameworks/QtQmlMeta.framework" \
       "$DST/Contents/Frameworks/QtQmlWorkerScript.framework" \
       "$DST/Contents/Frameworks/QtOpenGL.framework"

echo "=== 浏览器扩展进包(网页音频必需;从 dmg 拖出后"加载已解压的扩展程序")==="
cp -R browser-extension dist-stage/PixelPet-浏览器扩展

echo "=== 清残留 Homebrew rpath(双 Qt 加载崩隐患)==="
# macdeployqt 把 dylib 链接改写成 @executable_path/../Frameworks,但会留下构建机
# 的绝对 rpath(如 /opt/homebrew/opt/qt/lib)。开发机装了 Homebrew Qt 时,dyld 的
# @rpath 回退会命中它 → 双 Qt 加载(objc 双 class 警告,重则 exit 134)。
# 包内链接已全部指向 bundle,删掉绝对 rpath 无副作用。
find "$DST" \( -name '*.dylib' -o -name 'pixel-pet' \) -type f | while IFS= read -r f; do
    for rp in /opt/homebrew/opt/qt/lib /usr/local/opt/qt/lib; do
        if otool -l "$f" 2>/dev/null | grep -q "path $rp "; then
            install_name_tool -delete_rpath "$rp" "$f" 2>/dev/null || true
        fi
    done
done
cat > dist-stage/安装说明.txt <<'EOF'
PixelPet — 上网陪伴桌宠

【浏览器扩展(网页音频必需)】
macOS 上系统音频暂不支持,网页播放(任意站点)靠浏览器扩展:
1. 把 PixelPet-浏览器扩展 文件夹拖到桌面或下载目录(固定位置,别删)
2. 浏览器打开 chrome://extensions
3. 右上角开启「开发者模式」
4. 点「加载已解压的扩展程序」→ 选择该文件夹
5. 打开任意网页播放站点播放,点扩展图标开始抓音频(快捷键 Ctrl+Shift+0)

【使用】
拖 PixelPet.app 到「应用程序」;双击运行;本地/网页/系统音频(Windows)自动驱动宠物。

【许可】GPL-2.0
EOF

echo "=== ad-hoc 签名(本地分发,非 App Store)==="
codesign --force --deep --sign - "$DST" || true

echo "=== 打 .dmg(app + 扩展 + 安装说明)==="
DMG="dist/PixelPet-${VER}.dmg"
hdiutil create -volname "PixelPet" -srcfolder dist-stage -ov -format UDZO "$DMG" >/dev/null
echo "完成:$DMG($(du -h "$DMG" | cut -f1);stage 副本在 dist-stage/,可单独分发)"
rm -rf dist-stage
