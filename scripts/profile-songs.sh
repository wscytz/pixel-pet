#!/usr/bin/env bash
# 批量采集 samples/ 下歌曲的声学特征,供调参拟合。
# 每首跑 ~30s,收 stderr [dsp] 行的 rms/flat/low/cent 均值,按文件名前缀(情绪标签)汇总。
#
# 命名约定:<label>_<歌名>.mp3,label 用期望情绪档:
#   agitated(躁动) hype(热血) joyful(欢快) healing(治愈) calm(平静) sad(伤感) love(爱心)
# 例:agitated_dubstep1.mp3 / joyful_pop1.mp3 / healing_ballad1.mp3
# 每类放 2~3 首代表性强的。跑完看分布 → 手调 EmotionMapper 的 arousal/valence 系数 → 重跑验证。
set -e
cd "$(dirname "$0")/.."
BIN=./build/pixel-pet.app/Contents/MacOS/pixel-pet
SAMPLES=samples
mkdir -p "$SAMPLES" build/profile
SEC=${1:-30}   # 每首秒数,默认 30(可 ./profile-songs.sh 45)

shopt -s nullglob
files=("$SAMPLES"/*.*)
[ ${#files[@]} -gt 0 ] || { echo "samples/ 里没有歌;先放几首(<label>_<name>.mp3)"; exit 1; }

echo "=== 采集 ${#files[@]} 首,每首 ${SEC}s(窗口会逐个弹出,正常)==="
for f in "${files[@]}"; do
  base=$(basename "$f")
  log="build/profile/${base}.log"
  : > "$log"
  printf "  ▶ %-30s " "$base"
  "$BIN" "$f" >"$log" 2>&1 &
  pid=$!
  sleep "$SEC"
  kill "$pid" 2>/dev/null || true
  wait "$pid" 2>/dev/null || true
  echo "done"
done

echo
echo "=== 汇总:每首特征均值 + 期望标签(label)==="
printf "%-30s %-10s %7s %7s %7s %7s\n" file label rms flat low cent
for f in "${files[@]}"; do
  base=$(basename "$f")
  log="build/profile/${base}.log"
  label=${base%%_*}
  awk -v lb="$label" -v f="$base" '
    /\[dsp\]/ {
      for (i = 1; i <= NF; i++) {
        if (substr($i,1,4)=="rms=") { r += substr($i,5); rn++ }
        else if (substr($i,1,5)=="flat=") { fl += substr($i,6); fn++ }
        else if (substr($i,1,4)=="low=") { lo += substr($i,5); lon++ }
        else if (substr($i,1,5)=="cent=") { c += substr($i,6); cn++ }
      }
    }
    END {
      printf "%-30s %-10s %7.3f %7.3f %7.3f %7.3f\n", f, lb, (rn?r/rn:0), (fn?fl/fn:0), (lon?lo/lon:0), (cn?c/cn:0)
    }' "$log"
done
echo
echo "→ 看每类 label 的特征集中区域,对照 EmotionMapper 的 arousal/valence 公式调系数。"
