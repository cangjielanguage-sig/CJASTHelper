#!/usr/bin/env bash
# =============================================================
# gen_stdcore_sema_baseline.sh — 重新生成 std.core 语义结果对拍基线（多平台统一）
#
# 为什么需要它：旧基线（2026-09-22-stdcore-b120rc3-calibrated）只在生成机上可复现——
#   其 task.json 内是生成机的**绝对路径**（/home/tiny/.../B120RC3/...），换机器/换 SDK 即失效。
#   本脚本把「基线生成」变成**任何装了 SDK + 源码的机器都能跑**的确定性步骤：
#     SDK 源码根经环境变量解析（不写死路径），task 模板用相对路径，产物 + 清单一次生成。
#
# 口径（与 aiws assets 归档一致，勿随意改）：
#   stage=sema, enableDesugar=true, enableMacro=true, passes=['dump-semantic-result']
#   cjc 参数：--output-type=dylib --no-prelude --no-sub-pkg -O0 -Woff unused -Woff parser
#   输入：<stdlib 根>/libs/std/core/*.cj（60 文件，CMake 官方序）
#   产物：<out>/std.core.semantic-result + std.core.stats + filelist.txt（**相对路径**，供 TC driver）
#
# 用法：
#   source scripts/mac_env.sh        # 或 linux_env.sh：声明本机 SDK 配对
#   bash scripts/gen_stdcore_sema_baseline.sh <out-dir>
#
# 环境：
#   CJAH_STDLIB_ROOT  可选；SDK 源码侧的 runtime 根（其下需有 stdlib/libs/std/core）。默认由
#                     $CANGJIE_SRC_HOME/cangjie_runtime-*/stdlib 或 $CANGJIE_HOME/../cangjie_runtime-*/stdlib 探测。
#   SDK 选择         由 *_env.sh 声明（mac_env.sh → B120RC3）⇒ 决定 stdlib 源码根与构建期头/库路径。
# =============================================================
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${1:-$ROOT/output/sema_baseline}"
TASK_TEMPLATE="$ROOT/config/stdcore_sema_task.json"

[ -f "$TASK_TEMPLATE" ] || { echo "!! 缺 task 模板: $TASK_TEMPLATE"; exit 1; }
[ -x "$ROOT/build/bin/cjah" ] || { echo "!! 缺 cjah，请先构建: source scripts/mac_env.sh && bash build.sh -u -b"; exit 1; }

# ---- stdlib 根解析（多平台统一的关键：不写死机器路径）----
# 归一：接受两种写法——<runtime 根>（其下有 stdlib/libs/std/core）或 <stdlib 根>（其下有 libs/std/core）；
# task 模板内的相对路径以 <runtime 根> 为基准（`stdlib/libs/std/core/x.cj`）。
STDLIB_ROOT="${CJAH_STDLIB_ROOT:-}"
if [ -z "$STDLIB_ROOT" ]; then
    for cand in ${CANGJIE_SRC_HOME:-}/cangjie_runtime-* \
                ${CANGJIE_HOME:-}/../cangjie_runtime-* \
                ${CANGJIE_HOME:-}/../cangjie_runtime-*/stdlib; do
        if [ -d "$cand/stdlib/libs/std/core" ]; then STDLIB_ROOT="$cand"; break; fi
        if [ -d "$cand/libs/std/core" ]; then STDLIB_ROOT="$(dirname "$cand")"; break; fi
    done
fi
if [ -z "$STDLIB_ROOT" ] || [ ! -d "$STDLIB_ROOT/stdlib/libs/std/core" ]; then
    echo "!! 未能解析 stdlib 根（需含 stdlib/libs/std/core）——请显式设置 CJAH_STDLIB_ROOT"; exit 1
fi
echo "[gen] stdlib 根: $STDLIB_ROOT"

mkdir -p "$OUT"
# ---- task 模板（相对路径）→ 本机绝对路径 ----
python3 - "$TASK_TEMPLATE" "$STDLIB_ROOT" "$OUT/task.json" <<'PY'
import json, os, sys
tpl, stdlib_root, dst = sys.argv[1], sys.argv[2], sys.argv[3]
tasks = json.load(open(tpl))
for t in tasks:
    args, out = [], None
    for a in t["args"]:
        args.append(os.path.join(stdlib_root, a) if a.endswith(".cj") else a)
    t["args"] = args
    cj = [a for a in args if a.endswith(".cj")]
    miss = [a for a in cj if not os.path.isfile(a)]
    if miss:
        print("!! 缺源文件 %d 个（示例 %s）" % (len(miss), miss[0]), file=sys.stderr); sys.exit(3)
    t["args"][args.index("--output-dir") + 1] = os.path.abspath(os.path.dirname(dst))
    print("[gen] 源文件 %d 个；output-dir=%s" % (len(cj), t["args"][args.index("--output-dir") + 1]))
json.dump(tasks, open(dst, "w"), ensure_ascii=False, indent=2)
PY

# ---- 跑 cjah（SDK 动态库路径：macOS 走 DYLD_LIBRARY_PATH）----
if [ "$(uname)" = "Darwin" ]; then
    export DYLD_LIBRARY_PATH="${CANGJIE_HOME:-}/tools/lib:${CANGJIE_HOME:-}/runtime/lib/darwin_aarch64_cjnative:${CANGJIE_HOME:-}/lib/darwin_aarch64_cjnative:${DYLD_LIBRARY_PATH:-}"
fi
( cd "$OUT" && "$ROOT/build/bin/cjah" --task-config="$OUT/task.json" > "$OUT/cjah.log" 2>&1 ) \
    || { echo "!! cjah 失败，见 $OUT/cjah.log"; tail -20 "$OUT/cjah.log"; exit 1; }

RESULT="$OUT/std.core.semantic-result"
[ -s "$RESULT" ] || { echo "!! 未产出 $RESULT"; exit 1; }

# ---- filelist（相对路径，60 文件，与 task 同序）----
python3 - "$OUT/task.json" "$STDLIB_ROOT" "$OUT/filelist.txt" <<'PY'
import json, os, sys
task, stdlib_root, dst = sys.argv[1], sys.argv[2], sys.argv[3]
t = json.load(open(task))[0]
rel = [os.path.relpath(a, stdlib_root) for a in t["args"] if a.endswith(".cj")]
open(dst, "w").write("\n".join(rel) + "\n")
print("[gen] filelist: %d 行（相对路径）" % len(rel))
PY

echo "[gen] 产物就绪: $OUT"
echo "  $(tr '\n' ' ' < "$OUT/std.core.stats")"
if command -v shasum >/dev/null 2>&1; then
    echo "  sha256: $(shasum -a 256 "$RESULT" | awk '{print $1}')"
else
    echo "  sha256: $(sha256sum "$RESULT" | awk '{print $1}')"
fi
echo "  归档（口径变更时）：cp $OUT/std.core.{semantic-result,stats} <aiws>/CJASTHelper/reports/assets/<date>-stdcore-<sdk>.{semantic-result,stats}"
