#!/usr/bin/env bash
# macOS 打包:复制 .app → macdeployqt 打 Qt 依赖 → ad-hoc 签名 → .dmg。
# ★ 关键:在 dist/ 副本上打,绝不在 build/pixel-pet.app 上跑 macdeployqt
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
