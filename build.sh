#!/bin/bash

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

CWD=$(dirname $(realpath "$0"))
CJ_INC=$CANGJIE_SRC_HOME/include
# Use local third_party/json if JSON_PATH not set
JSON_INC=${JSON_PATH:-"$CWD/third_party/json"}
BTYPE=Debug
BUILD_DIR=$CWD/build
SOURCE_DIR=$CWD
NINJA_BIN=ninja
VERBOSE=
EXT=
CJAH=$BUILD_DIR/bin/cjah$EXT
TEST_RUNNER=$BUILD_DIR/bin/cjah_test$EXT
PRE=$CWD/output
TEST=OFF
CANGJIE_LIB=$CANGJIE_HOME/tools/lib
ALONE=OFF

function print_header() {
    echo -e "${CYAN}==========================================${NC}"
    echo -e "${CYAN}     CJASTHelper Build System${NC}"
    echo -e "${CYAN}==========================================${NC}"
}

function print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

function print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

function print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

function print_error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
}

function run_cmd() {
    if [ $# -eq 0 ]; then
        print_error "No commands provided"
        return 1
    fi
    if [ -n "$VERBOSE" ]; then
        echo -e "${PURPLE}[CMD]${NC} $@"
    fi
    "$@"
}

# update cmake cache 
function update() {
    print_info "Updating CMake cache..."
    print_info "Library path: $CANGJIE_LIB"
    run_cmd cmake -G "Ninja" -B $BUILD_DIR -S $SOURCE_DIR \
        -DJSON_INCLUDE=$JSON_INC \
        -DCANGJIE_INCLUDE=$CJ_INC \
        -DCANGJIE_LIB=$CANGJIE_LIB \
        -DCMAKE_BUILD_TYPE=$BTYPE \
        -DCMAKE_INSTALL_PREFIX=$PRE \
        -DCMAKE_ENABLE_TEST=$TEST \
        -DCMAKE_C_COMPILER=clang \
        -DCMAKE_CXX_COMPILER=clang++
    
    if [ $? -eq 0 ]; then
        print_success "CMake cache updated successfully"
    else
        print_error "Failed to update CMake cache"
        return 1
    fi
}

function build() {
    print_header
    print_info "Building ${BTYPE} version..."
    
    # update cmake
    update
    if [ $? -ne 0 ]; then
        return 1
    fi
    
    print_info "Starting build process..."
    run_cmd $NINJA_BIN -C $BUILD_DIR $VERBOSE
    
    if [ $? -eq 0 ]; then
        print_success "Build completed successfully"
    else
        print_error "Build failed"
        return 1
    fi
}

# ==============================================================================
# alone 安装模式：把 SDK 提供的运行时库拷进安装目录，使安装产物不再依赖
# CANGJIE_HOME / LD_LIBRARY_PATH / DYLD_LIBRARY_PATH 等外部配置即可运行。
# 当前工程直接链接的 SDK 动态库只有仓颉前端库 cangjie-lsp（CMakeLists.txt 中的
# IMPORTED 目标），其余均为系统库；Windows 侧由 build.ps1 处理（DLL 须与 exe 同目录）。
# ==============================================================================

# 平台对应的前端库文件名
function sdk_frontend_lib_name() {
    case "$(uname -s)" in
        Darwin) echo "libcangjie-lsp.dylib" ;;
        Linux)  echo "libcangjie-lsp.so" ;;
        *)      echo "" ;;
    esac
}

# macOS：链接时记录的是 SDK 内的绝对 install_name，拷贝出的副本不会被命中，
# 需改写为 @rpath 引用并确保 rpath 存在，最后重做 adhoc 签名（arm64 必需）。
function fixup_macos_alone_deps() {
    local lib_name=$1
    local sdk_ref="$CANGJIE_LIB/$lib_name"
    local target rpath

    if ! command -v install_name_tool >/dev/null 2>&1; then
        print_warning "install_name_tool not found, skip install-name fixup"
        return 0
    fi

    for target in "$PRE"/bin/cjah "$PRE"/lib/*.dylib; do
        [ -f "$target" ] || continue
        case "$target" in
            */bin/*) rpath="@loader_path/../lib" ;;
            *)       rpath="@loader_path" ;;
        esac

        if otool -L "$target" 2>/dev/null | grep -qF "$sdk_ref"; then
            run_cmd install_name_tool -change "$sdk_ref" "@rpath/$lib_name" "$target" || return 1
        fi
        # -add_rpath 对已存在的 rpath 会报错，此处忽略（幂等）
        run_cmd install_name_tool -add_rpath "$rpath" "$target" 2>/dev/null
        if command -v codesign >/dev/null 2>&1; then
            run_cmd codesign --force --sign - "$target" >/dev/null 2>&1
        fi
    done
    print_info "Rewrote cangjie-lsp reference to @rpath/$lib_name"
}

# Linux：用 ldd 校验安装产物已无未解析动态库（依赖闭环）
function check_alone_closure() {
    if ! command -v ldd >/dev/null 2>&1; then
        return 0
    fi
    local missing
    missing=$( { ldd "$PRE"/bin/cjah; ldd "$PRE"/lib/*.so; } 2>/dev/null | grep "not found" || true )
    if [ -n "$missing" ]; then
        print_warning "Unresolved shared libraries after alone install:"
        echo "$missing"
        return 1
    fi
    print_info "Dependency closure verified: no unresolved shared libraries"
}

function deploy_alone_deps() {
    local lib_name lib_src
    lib_name=$(sdk_frontend_lib_name)
    if [ -z "$lib_name" ]; then
        print_error "Alone mode is not supported on $(uname -s); use build.ps1 on Windows"
        return 1
    fi

    print_info "Alone mode: bundling SDK runtime libs into $PRE"
    lib_src="$CANGJIE_LIB/$lib_name"
    if [ ! -f "$lib_src" ]; then
        print_error "Dependency library not found: $lib_src (check -d / CANGJIE_HOME)"
        return 1
    fi

    run_cmd mkdir -p "$PRE/lib"
    run_cmd cp -f "$lib_src" "$PRE/lib/"
    if [ $? -ne 0 ]; then
        print_error "Failed to copy $lib_name into $PRE/lib"
        return 1
    fi
    print_success "Deployed $lib_name -> $PRE/lib"

    if [ "$(uname -s)" == "Darwin" ]; then
        fixup_macos_alone_deps "$lib_name" || return 1
    else
        check_alone_closure || return 1
    fi
}

function install() {
    print_header
    print_info "Installing binaries..."
    
    if [ ! -f "$CJAH" ]; then
        print_warning "Binary not found, building first..."
        build
        if [ $? -ne 0 ]; then
            return 1
        fi
    fi
    
    # 用 cmake --install 显式传 --prefix：`ninja install` 只认 configure 时的
    # CMAKE_INSTALL_PREFIX，会让 -p <dir> 与 alone 部署目录不一致。
    local install_args=(--install "$BUILD_DIR" --prefix "$PRE")
    if [ -n "$VERBOSE" ]; then
        install_args+=(--verbose)
    fi
    run_cmd cmake "${install_args[@]}"

    if [ $? -ne 0 ]; then
        print_error "Installation failed"
        return 1
    fi

    if [ "$ALONE" == "ON" ]; then
        deploy_alone_deps || return 1
        print_success "Installation completed (alone mode, dependencies closed)"
    else
        print_success "Installation completed successfully"
        print_info "Tip: add -a to bundle SDK runtime libs into the install dir (alone mode)"
    fi
}

function run() {
    if [ ! -f "$CJAH" ]; then
        print_warning "Binary not found, building first..."
        build
        if [ $? -ne 0 ]; then
            return 1
        fi
    fi
    
    if [ -f "$CJAH" ]; then
        print_info "Running cjah..."
        run_cmd $CJAH $@
    fi
}

function test() {
    print_header
    print_info "Running tests..."
    
    if [ ! -f "$TEST_RUNNER" ]; then
        print_warning "Test runner not found, building first..."
        build
        if [ $? -ne 0 ]; then
            return 1
        fi
    fi
    
    run_cmd $TEST_RUNNER $@
}

# help:
#    bash build.sh -b -r -u [run_args]
function help() {
    echo -e "${CYAN}Usage:${NC}"
    echo -e "    bash build.sh [options] -- [runargs]"
    echo ""
    echo -e "${CYAN}Examples:${NC}"
    echo -e "    bash build.sh -v -t Debug -b"
    echo -e "    bash build.sh -t Release -b"
    echo -e "    bash build.sh -v -r -- test/main.cj"
    echo ""
    echo -e "${CYAN}Options:${NC}"
    echo -e "    ${GREEN}-h${NC} Show this help message"
    echo -e "    ${GREEN}-v${NC} Dump build verbose info"
    echo -e "    ${GREEN}-g${NC} Enable test with googletest"
    echo -e "    ${GREEN}-t${NC} Config build type [Debug | Release]"
    echo -e "    ${GREEN}-p${NC} Config install prefix"
    echo -e "    ${GREEN}-d${NC} Set dependent lib path"
    echo -e "    ${GREEN}-u${NC} Update cmake cache"
    echo -e "    ${GREEN}-b${NC} Build only"
    echo -e "    ${GREEN}-i${NC} Install binary"
    echo -e "    ${GREEN}-a${NC} Alone install: bundle SDK runtime libs into install dir"
    echo -e "    ${GREEN}-c${NC} Clean build"
    echo -e "    ${GREEN}-r${NC} Run binary or test, optional with args '-- [runargs]'"
}

function main() {
    action=
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -h) help; exit 0 ;;
            -v) VERBOSE="-v"; shift ;;
            -g) TEST="ON"; shift ;;
            -t) BTYPE="$2"; shift 2 ;;
            -p) PRE="$2"; shift 2 ;;
            -d) CANGJIE_LIB="$2"; shift 2 ;;
            -b) action="build"; shift ;;
            -u) action="update"; shift ;;
            -i) action="install"; shift ;;
            -a) ALONE="ON"; shift ;;
            -c) action="clean"; shift ;;
            -r)
                action="run"
                if [[ "$TEST" == "ON" ]]; then
                    action="test"
                fi
                shift ;;
            --) shift; break ;;
            *) print_error "Unknown parameter: $1"; exit 1 ;;
        esac
    done

    print_info "Action: $action, Run args: $@"
    case "X$action" in
        Xupdate) update ;;
        Xbuild) build ;;
        Xinstall) install ;;
        Xclean) 
            print_info "Cleaning build directory..."
            run_cmd rm -rf build output
            print_success "Clean completed"
            ;;
        Xrun) run $@ ;;
        Xtest) test $@ ;;
        *) 
            if [ -z "$action" ]; then
                print_header
                print_info "No action specified. Use -h for help."
            fi
            ;;
    esac
}

main $*