#!/bin/bash
#
# CJASTHelper 发布打包脚本
#
# 把 `bash build.sh -a -i` 产出的安装目录按版本号打成可分发的 tar.gz：
#   dist/cjah_<version>.tar.gz          完整版（bin/ lib/ config/，含 pass 插件）
#   dist/cjah_minimal_<version>.tar.gz  最小版（仅语法检查：裁掉插件，选项只留 check-syntax）
#
# 用法:
#   bash scripts/package_release.sh -v alpha_0.0.7
#   bash scripts/package_release.sh -v alpha_0.0.7 --minimal
#
# 选项:
#   -v, --version <ver>  版本号（必填），如 alpha_0.0.7
#   -m, --minimal        生成最小版（只支持 --check-syntax，裁掉 pass 插件）
#   -p, --prefix <dir>   安装目录（默认 <repo>/output，由 build.sh -a -i 产出）
#   -o, --out <dir>      打包输出目录（默认 <repo>/dist）
#   -k, --keep           保留暂存目录（默认打完成包后删除）
#   -h, --help           显示帮助
#
# 依赖: bash / tar / python3（裁剪 valid_options.json）/ ldd|otool（依赖闭环自检）
#
set -uo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

ROOT=$(cd "$(dirname "$(realpath "$0")")/.." && pwd)

VERSION=""
MINIMAL=OFF
PREFIX="$ROOT/output"
OUT_DIR="$ROOT/dist"
KEEP=OFF

function info() { echo -e "${BLUE}[INFO]${NC} $*"; }
function ok() { echo -e "${GREEN}[ OK ]${NC} $*"; }
function warn() { echo -e "${YELLOW}[WARN]${NC} $*" >&2; }
function err() { echo -e "${RED}[FAIL]${NC} $*" >&2; }
function die() {
    err "$*"
    exit 1
}

function usage() {
    sed -n '2,24p' "$0" | sed 's/^#\{1,2\} \{0,1\}//'
}

function parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -v | --version)
                VERSION="${2:-}"
                shift 2 || die "--version 缺少参数"
                ;;
            -m | --minimal)
                MINIMAL=ON
                shift
                ;;
            -p | --prefix)
                PREFIX="${2:-}"
                shift 2 || die "--prefix 缺少参数"
                ;;
            -o | --out)
                OUT_DIR="${2:-}"
                shift 2 || die "--out 缺少参数"
                ;;
            -k | --keep)
                KEEP=ON
                shift
                ;;
            -h | --help)
                usage
                exit 0
                ;;
            *)
                die "未知参数: $1（-h 查看用法）"
                ;;
        esac
    done

    [[ -n "$VERSION" ]] || die "必须用 -v/--version 指定版本号，如 -v alpha_0.0.7"
    # 版本号只允许出现在目录/文件名里：字母数字与 . _ -
    [[ "$VERSION" =~ ^[A-Za-z0-9][A-Za-z0-9._-]*$ ]] ||
        die "版本号 '$VERSION' 非法：仅允许字母、数字、'.'、'_'、'-'，且不能以符号开头"

    # 相对路径按调用目录解析，绝对路径直接用
    [[ "$PREFIX" = /* ]] || PREFIX="$PWD/$PREFIX"
    [[ "$OUT_DIR" = /* ]] || OUT_DIR="$PWD/$OUT_DIR"
}

# 检查安装目录是否可用（并提示缺失原因）
function check_prefix() {
    [[ -d "$PREFIX" ]] || die "安装目录不存在: $PREFIX（先执行 bash build.sh -a -i）"
    [[ -x "$PREFIX/bin/cjah" ]] || die "未找到可执行文件: $PREFIX/bin/cjah（先执行 bash build.sh -a -i）"
    [[ -d "$PREFIX/config" ]] || die "安装目录缺少 config/: $PREFIX/config"
    [[ -d "$PREFIX/lib" ]] || die "安装目录缺少 lib/: $PREFIX/lib"

    local lsp
    lsp=$(find "$PREFIX/lib" -maxdepth 1 -name 'libcangjie-lsp.*' 2>/dev/null | head -1)
    if [[ -z "$lsp" ]]; then
        die "安装目录缺少 libcangjie-lsp.*（打包后无法脱离 SDK 运行），请用 bash build.sh -a -i 重新安装"
    fi
}

# 依赖闭环自检：安装/裁剪后的产物不应再有未解析的动态库
function check_closure() {
    local dir=$1
    case "$(uname -s)" in
        Linux)
            command -v ldd >/dev/null 2>&1 || return 0
            local missing
            missing=$(ldd "$dir/bin/cjah" 2>/dev/null | grep "not found" || true)
            if [[ -n "$missing" ]]; then
                err "存在未解析的动态库:"
                echo "$missing" >&2
                return 1
            fi
            ;;
        Darwin)
            command -v otool >/dev/null 2>&1 || return 0
            # alone 模式下引用应为 @rpath/@loader_path 或系统路径；
            # 出现 SDK 内的绝对路径说明打包后仍依赖外部 SDK
            local bad
            bad=$(otool -L "$dir/bin/cjah" | awk 'NR>1 {print $1}' |
                grep -vE '^(@rpath/|@loader_path/|/usr/lib/|/System/)' || true)
            if [[ -n "$bad" ]]; then
                err "存在指向包外的动态库引用:"
                echo "$bad" >&2
                return 1
            fi
            ;;
        *) ;;
    esac
    return 0
}

# 最小版：裁掉 pass 插件 + 选项只留 check-syntax
function make_minimal() {
    local dir=$1

    # 1) pass 插件（check-syntax 只跑 parse 阶段，不走任何 pass）
    #    Linux/macOS 插件名 libcjast_*_pass.so|dylib 且装到 lib/；
    #    Windows(MinGW) 去掉了前缀、装到 bin/（cjast_*_pass.dll），两种布局都覆盖
    local f
    for f in "$dir"/lib/libcjast_*_pass.* "$dir"/bin/cjast_*_pass.dll "$dir"/bin/libcjast_*_pass.dll; do
        [[ -e "$f" ]] || continue
        rm -f "$f"
        info "裁剪插件: $(basename "$f")"
    done
    info "保留动态库: $(ls "$dir/lib" 2>/dev/null | tr '\n' ' ')"

    # 2) 任务/流程配置与 check-syntax 无关，一并裁掉
    rm -f "$dir/config/tasks.json" "$dir/config/stdcore_sema_task.json"

    # 3) valid_options.json 只保留 check-syntax（从安装目录的原始文件裁剪，避免漂移）
    python3 - "$dir/config/valid_options.json" <<'PY' || return 1
import json
import sys

path = sys.argv[1]
with open(path, encoding="utf-8") as f:
    opts = json.load(f)

keep = [o for o in opts if o.get("key") == "check-syntax"]
if len(keep) != 1:
    sys.exit(f"valid_options.json 中未找到唯一的 check-syntax 条目: {[o.get('key') for o in opts]}")

with open(path, "w", encoding="utf-8") as f:
    json.dump(keep, f, indent=2, ensure_ascii=False)
    f.write("\n")
print("kept options:", [o["key"] for o in keep])
PY
    # config/passes.json 必须保留：ArgHelper 启动时无条件解析它（PassManager::Init）
    [[ -f "$dir/config/passes.json" ]] || die "最小版缺少 config/passes.json（启动即会失败）"
    return 0
}

# 打包前自检：按实际发布形态跑真实用例
function self_check() {
    local dir=$1
    local tmp
    tmp=$(mktemp -d)

    printf 'func add(a: Int64, b: Int64): Int64 {\n    return a + b\n}\n' >"$tmp/ok.cj"
    printf 'func broken( {\n' >"$tmp/bad.cj"
    mkdir -p "$tmp/multi/pkgA" "$tmp/multi/pkgB"
    printf 'package pkgA\nfunc a(): Unit {}\n' >"$tmp/multi/pkgA/a.cj"
    printf 'package pkgB\nfunc b(): Unit {}\n' >"$tmp/multi/pkgB/b.cj"

    local run=(env -i "$dir/bin/cjah")
    command -v env >/dev/null 2>&1 || run=("$dir/bin/cjah")

    # 1) 语法正确的单文件：必须 0
    "${run[@]}" --check-syntax=true "$tmp/ok.cj" >/dev/null 2>&1 ||
        { err "自检失败: 语法正确用例返回非 0"; return 1; }
    ok "自检: 单文件语法检查通过"

    # 2) 语法错误的单文件：必须非 0
    if "${run[@]}" --check-syntax=true "$tmp/bad.cj" >/dev/null 2>&1; then
        err "自检失败: 语法错误用例返回了 0"
        return 1
    fi
    ok "自检: 语法错误用例被正确判错（退出码非 0）"

    # 3) 目录输入（多包分组）：必须 0
    "${run[@]}" --check-syntax=true "$tmp/multi" >/dev/null 2>&1 ||
        { err "自检失败: 目录（多包）输入返回非 0"; return 1; }
    ok "自检: 目录（多包）输入通过"

    # 3b) 目录内含语法错误文件：必须非 0
    printf 'func x( {\n' >"$tmp/multi/pkgB/bad.cj"
    if "${run[@]}" --check-syntax=true "$tmp/multi" >/dev/null 2>&1; then
        err "自检失败: 目录内含语法错误文件却返回 0"
        rm -f "$tmp/multi/pkgB/bad.cj"
        return 1
    fi
    rm -f "$tmp/multi/pkgB/bad.cj"
    ok "自检: 目录内含语法错误被正确判错"

    # 3c) `--output-dir <dir>` 两段式写法（历史 bug：取值被当成输入目录 ⇒ 假通过）
    mkdir -p "$tmp/outdir"
    if "${run[@]}" --check-syntax=true --output-dir "$tmp/outdir" "$tmp/bad.cj" >/dev/null 2>&1; then
        err "自检失败: '--output-dir <dir>' 两段式写法下语法错误未被判错（假通过）"
        return 1
    fi
    ok "自检: '--output-dir <dir>' 两段式写法仍能判错"

    # 3d) 只读当前目录（历史 bug：前端写不进去 ⇒ 一律返回 0）
    if [ "$(id -u)" -ne 0 ]; then
        mkdir -p "$tmp/ro"
        if chmod 555 "$tmp/ro" 2>/dev/null; then
            if (cd "$tmp/ro" && "${run[@]}" --check-syntax=true "$tmp/bad.cj" >/dev/null 2>&1); then
                err "自检失败: 只读当前目录下语法错误未被判错（假通过）"
                chmod 755 "$tmp/ro"
                return 1
            fi
            if ! (cd "$tmp/ro" && "${run[@]}" --check-syntax=true "$tmp/ok.cj" >/dev/null 2>&1); then
                err "自检失败: 只读当前目录下正确用例返回非 0"
                chmod 755 "$tmp/ro"
                return 1
            fi
            chmod 755 "$tmp/ro"
            ok "自检: 只读当前目录下判定正确（自动改用临时输出目录）"
        fi
    fi

    # 4) 选项可见性：最小版不得出现 check-syntax 之外的选项
    local help_text
    help_text=$("${run[@]}" --help 2>&1 || true)
    grep -q -- "--check-syntax" <<<"$help_text" || { err "自检失败: --help 未列出 check-syntax"; return 1; }
    if [[ "$MINIMAL" == "ON" ]]; then
        # 只比对「选项清单」里的条目（行首缩进 + --key），
        # 因为 check-syntax 的描述里本身会提到 --dump-source
        local extra
        extra=$(grep -oE '^ +--[a-z][a-z-]*' <<<"$help_text" | tr -d ' ' |
            sort -u | grep -v '^--check-syntax$' || true)
        if [[ -n "$extra" ]]; then
            err "自检失败: 最小版仍暴露其它选项:"
            echo "$extra" >&2
            return 1
        fi
        ok "自检: 选项仅剩 --check-syntax"
        # 5) 插件确实已裁掉（lib/ 与 bin/ 两种布局都查）
        if find "$dir/lib" "$dir/bin" -maxdepth 1 \
            \( -name 'libcjast_*_pass.*' -o -name 'cjast_*_pass.dll' \) | grep -q .; then
            err "自检失败: 最小版仍含 pass 插件"
            return 1
        fi
        ok "自检: 无 pass 插件残留"
    fi

    # 6) 依赖闭环（不依赖外部 SDK / LD_LIBRARY_PATH）
    check_closure "$dir" || {
        rm -rf "$tmp"
        return 1
    }
    ok "自检: 动态库依赖闭环（无未解析依赖）"
    rm -rf "$tmp"
    return 0
}

function main() {
    parse_args "$@"

    local name="cjah_${VERSION}"
    [[ "$MINIMAL" == "ON" ]] && name="cjah_minimal_${VERSION}"

    check_prefix
    mkdir -p "$OUT_DIR"

    local stage="$OUT_DIR/$name"
    info "版本号: $VERSION"
    info "安装目录: $PREFIX"
    info "输出目录: $OUT_DIR"
    info "包名: $name（$([[ $MINIMAL == ON ]] && echo 最小版/仅语法检查 || echo 完整版)）"

    rm -rf "$stage"
    mkdir -p "$stage"
    cp -a "$PREFIX/bin" "$PREFIX/lib" "$PREFIX/config" "$stage/"

    # 随包附带版本信息与许可证（便于分发后溯源）
    printf '%s\n' "$VERSION" >"$stage/VERSION"
    for f in LICENSE README.md; do
        [[ -f "$ROOT/$f" ]] && cp "$ROOT/$f" "$stage/"
    done

    if [[ "$MINIMAL" == "ON" ]]; then
        make_minimal "$stage" || die "最小版裁剪失败"
    fi

    self_check "$stage" || { err "自检未通过，暂存目录保留以便排查: $stage"; exit 1; }

    local tarball="$OUT_DIR/$name.tar.gz"
    rm -f "$tarball"
    tar -czf "$tarball" -C "$OUT_DIR" "$name" || die "tar 打包失败"
    [[ "$KEEP" == "ON" ]] || rm -rf "$stage"

    local size sha
    size=$(du -h "$tarball" | cut -f1)
    if command -v sha256sum >/dev/null 2>&1; then
        sha=$(sha256sum "$tarball" | cut -d' ' -f1)
    elif command -v shasum >/dev/null 2>&1; then
        sha=$(shasum -a 256 "$tarball" | cut -d' ' -f1)
    else
        sha="(无 sha256 工具)"
    fi

    echo
    ok "打包完成: $tarball"
    echo "     大小: $size"
    echo "     sha256: $sha"
    echo "     解包即用: tar -xzf $(basename "$tarball") && ./$name/bin/cjah --check-syntax=true <file.cj>"
}

main "$@"
