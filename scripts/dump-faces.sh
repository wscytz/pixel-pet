#!/usr/bin/env bash
# dump-faces.sh — 逐档渲染像素脸自检(P0 行为门)。
# 断言:每个 tier 的脸(gridAscii 的 '#' 像素)非空,且任意两档的脸不完全相同。
# 防的是:扩 kTierCount 时漏配 spec 行 → 多档静默塌成同一张脸(编译器不会报错)。
# 走 --dump(与活窗口同一条 paint 路径),无头可跑。
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/build/pixel-pet.app/Contents/MacOS/pixel-pet"
[ -x "$BIN" ] || BIN="$ROOT/build/pixel-pet"          # 非 bundle 平台兜底
OUT="$ROOT/build/face-check"
mkdir -p "$OUT"

TIERS=$(grep -oE 'kTierCount *= *[0-9]+' "$ROOT/src/emotion/Emotion.h" | grep -oE '[0-9]+')
GENRES=$(grep -oE 'kGenreCount *= *[0-9]+' "$ROOT/src/emotion/Emotion.h" | grep -oE '[0-9]+')
echo "kTierCount=$TIERS kGenreCount=$GENRES → $OUT/"

# 脸只随 tier 变(genre 只影响底色/节拍),故取 genre 0 做「非空 + 互异」判定。
declare -a fp
for ((t = 0; t < TIERS; t++)); do
  grid=$("$BIN" --dump "$t" 0 "$OUT/t${t}_g0.png" 2>&1 >/dev/null | grep -E '^[.#+]+$' || true)
  nface=$(printf '%s' "$grid" | tr -cd '#' | wc -c | tr -d ' ')
  if [ "$nface" -eq 0 ]; then
    echo "FAIL: tier $t 脸为空(无 '#' 像素)" >&2
    exit 1
  fi
  fp[$t]=$(printf '%s' "$grid" | shasum | awk '{print $1}')
  # 其余 genre 的 PNG 也出一份,供目检配色/音浪
  for ((g = 1; g < GENRES; g++)); do
    "$BIN" --dump "$t" "$g" "$OUT/t${t}_g${g}.png" >/dev/null 2>&1 || true
  done
done

# 两两不同
for ((a = 0; a < TIERS; a++)); do
  for ((b = a + 1; b < TIERS; b++)); do
    if [ "${fp[$a]}" = "${fp[$b]}" ]; then
      echo "FAIL: tier $a 与 tier $b 的脸完全相同(spec 行漏配?)" >&2
      exit 1
    fi
  done
done

echo "OK: $TIERS 档脸非空且两两不同;PNG 在 $OUT/"
