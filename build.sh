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
JSON_INC=$JSON_PATH
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
        -DCJAH_STANDALONE=$ALONE \
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
    
    run_cmd $NINJA_BIN -C $BUILD_DIR $VERBOSE install
    
    if [ $? -eq 0 ]; then
        print_success "Installation completed successfully"
    else
        print_error "Installation failed"
        return 1
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
    echo -e "    ${GREEN}-a${NC} Enable standalone version"
    echo -e "    ${GREEN}-t${NC} Config build type [Debug | Release]"
    echo -e "    ${GREEN}-p${NC} Config install prefix"
    echo -e "    ${GREEN}-d${NC} Set dependent lib path"
    echo -e "    ${GREEN}-u${NC} Update cmake cache"
    echo -e "    ${GREEN}-b${NC} Build only"
    echo -e "    ${GREEN}-i${NC} Install binary"
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
            -a) ALONE="ON"; shift ;;
            -t) BTYPE="$2"; shift 2 ;;
            -p) PRE="$2"; shift 2 ;;
            -d) CANGJIE_LIB="$2"; shift 2 ;;
            -b) action="build"; shift ;;
            -u) action="update"; shift ;;
            -i) action="install"; shift ;;
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