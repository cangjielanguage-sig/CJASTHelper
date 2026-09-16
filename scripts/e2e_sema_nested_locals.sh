#!/bin/bash
# CJAH-7b: 嵌套作用域局部 var 收集 E2E 断言
# 用法: 从仓根 CWD 运行  bash scripts/e2e_sema_nested_locals.sh
# 依赖: build/bin/cjah 已构建 + CANGJIE_HOME 就绪（scripts/linux_env.sh）
set -euo pipefail

CJAH=build/bin/cjah
OUT_DIR=$(mktemp -d)
trap 'rm -rf "$OUT_DIR"' EXIT

cat > "$OUT_DIR/task.json" <<EOF
[{"stage": "sema", "enableDesugar": false, "enableMacro": false,
  "filterDecls": [], "ignoreDecls": [], "ignoreAnnotations": [],
  "passes": ["dump-semantic-result"],
  "args": ["test/data/inputs/nested_locals.cj", "--output-dir", "$OUT_DIR"]}]
EOF

"$CJAH" --task-config="$OUT_DIR/task.json" --output-type=dylib --no-prelude --no-sub-pkg -O0 >/dev/null 2>&1

RESULT="$OUT_DIR/nested_locals.semantic-result"
[ -f "$RESULT" ] || { echo "FAIL: semantic-result not produced"; exit 1; }

# 嵌套块局部 var 必须全部入 sym 段（CJAH-7a 验收）
EXPECTED="var#top var#innerIf var#innerFor var#innerElse var#innerWhile var#innerMatch var#innerTry var#innerCatch"
SYM_SECTION=$(sed -n '/^sym:/,/^bindings:/p' "$RESULT")
for name in $EXPECTED; do
    echo "$SYM_SECTION" | grep -qE "^S[0-9]+@[0-9]+: ${name}#ty:T[0-9]+" \
        || { echo "FAIL: sym missing $name"; exit 1; }
done
echo "PASS: nested locals all in sym ($EXPECTED)"
