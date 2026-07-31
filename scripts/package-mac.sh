#!/usr/bin/env bash
# macOS 打包:复制 .app → macdeployqt 打 Qt 依赖 → ad-hoc 签名 → .dmg。
# ★ 关键:在 dist/ 副本上打,绝不在 build/pixel-pet.app 上跑 macdeployqt
#   (它会往 app 里复制 frameworks + 改 rpath;之后改代码重 link 会双 Qt 加载崩,exit 134)。
set -euo pipefail
cd "$(dirname "$0")/.."

APP=build/pixel-pet.app
[ -d "$APP" ] || { echo "先构建:cmake --build build -j"; exit 1; }

# Qt bin(macdeployqt 所在)
QT_BIN="$(dirname "$(command -v qmake6 || command -v qmake || true)" 2>/dev/null)"
[ -n "$QT_BIN" ] || QT_BIN=/opt/homebrew/opt/qt/bin
[ -x "$QT_BIN/macdeployqt" ] || { echo "找不到 macdeployqt(改 QT_BIN 或 brew install qt)"; exit 1; }

VER="$(/usr/libexec/PlistBuddy -c 'Print CFBundleShortVersionString' "$APP/Contents/Info.plist" 2>/dev/null || echo 0.1.0)"
rm -rf dist
mkdir -p dist
DST="dist/PixelPet.app"
cp -R "$APP" "$DST"

echo "=== macdeployqt(Qt 依赖入 app)==="
"$QT_BIN/macdeployqt" "$DST" -verbose=1
echo "=== ad-hoc 签名(本地分发,非 App Store)==="
codesign --force --deep --sign - "$DST" || true

echo "=== 打 .dmg ==="
DMG="dist/PixelPet-${VER}.dmg"
hdiutil create -volname "PixelPet" -srcfolder "$DST" -ov -format UDZO "$DMG" >/dev/null
echo "完成:$DMG($DST 同名目录可单独分发)"
