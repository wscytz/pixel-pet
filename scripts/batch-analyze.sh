#!/usr/bin/env bash
# 批量分析歌曲情绪判据(下一批验证用):mp3 → afconvert → analyze 头less 秒跑。
# 用法:./scripts/batch-analyze.sh [歌曲目录/文件...]
#   默认扫描 ./samples/;也可传具体文件或目录。
#   每首输出 [sum](val/aro/mode/margin/小调占比)+ [dist](情绪档分布)+ [ms](调式模糊分桶)。
# 命名约定(可选):<label>_<歌名>.mp3,label 是期望情绪,便于对照。
set -e
cd "$(dirname "$0")/.."
BIN=./build/analyze
WAVDIR=/tmp/pp-batch
mkdir -p "$WAVDIR"

files=()
for arg in "$@"; do
  if [ -d "$arg" ]; then
    for f in "$arg"/*.*; do [ -f "$f" ] && files+=("$f"); done
  elif [ -f "$arg" ]; then
    files+=("$arg")
  fi
done
if [ ${#files[@]} -eq 0 ]; then
  shopt -s nullglob
  files=(samples/*.*)
fi
[ ${#files[@]} -gt 0 ] || { echo "没有输入文件;传目录/文件,或放歌到 samples/"; exit 1; }

for f in "${files[@]}"; do
  base=$(basename "$f")
  wav="$WAVDIR/${base%.*}.wav"
  case "$f" in
    *.wav) cp "$f" "$wav" ;;
    *) afconvert -f WAVE -d LEI16 -c 1 "$f" "$wav" >/dev/null 2>&1 || { echo "✗ 转换失败: $f"; continue; } ;;
  esac
  echo "━━━ $base"
  ./build/analyze "$wav" 2>&1 | grep -E '\[sum\] frames|\[sum\] key|\[ms\]|\[dist\]' \
    | sed 's/\[sum\] frames/  总和/; s/\[sum\] key/  key/; s/\[ms\] /  调式/; s/\[dist\] /  档 /'
done
