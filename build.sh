# source cangjie

CWD=$(dirname $(realpath "$0"))
CJ_INC=$CANGJIE_SRC_HOME/include
BTYPE=Debug
BUILD_DIR=$CWD/build
SOURCE_DIR=$CWD
NINJA_BIN=ninja
VERBOSE=
EXT=
CJH=$BUILD_DIR/bin/cjah$EXT
TEST_RUNNER=$BUILD_DIR/bin/cjah_test$EXT
PRE=$PWD/output
TEST=OFF


function run_cmd() {
    if [ $# -eq 0 ]; then
        echo "Error: no comands" >&2
        return 1
    fi
    if [ -n "$VERBOSE" ]; then
        echo "$@"
    fi
    "$@"
}

# update cmake cache 
function update() {
    run_cmd cmake -G "Ninja" -B $BUILD_DIR -S $SOURCE_DIR -DCANGJIE_INCLUDE=$CJ_INC -DCMAKE_BUILD_TYPE=$BTYPE -DCMAKE_INSTALL_PREFIX=$PRE -DCMAKE_ENABLE_TEST=$TEST -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
}

function build() {
    echo "Notice: you are building **${BTYPE}** version"
    # update cmake
    update
    run_cmd $NINJA_BIN -C $BUILD_DIR $VERBOSE
}

function install() {
    if [ ! -f "$CJH" ]; then
        build
    fi
    run_cmd $NINJA_BIN -C $BUILD_DIR $VERBOSE install
}

function run() {
    if [ ! -f "$CJH" ]; then
        build
    fi
    if [ -f "$CJH" ]; then
        run_cmd $CJH $@
    fi
}

function test() {
    if [ ! -f "$TEST_RUNNER" ]; then
        build
    fi
    run_cmd $TEST_RUNNER $@
}

# help:
#    bash build.sh -b -r -u [run_args]
function help() {
    echo "Usage:"
    echo "    bash build.sh [options] -- [runargs]"
    echo ""
    echo "For example:"
    echo "    bash build.sh -v -t Debug -b"
    echo "    bash build.sh -t Release -b"
    echo "    bash build.sh -v -r -- test/main.cj"
    echo ""
    echo "Options: "
    echo "    -v dump build verbose info"
    echo "    -g enable test with googletest"
    echo "    -t config build type, optional [Debug | Release]"
    echo "    -p config install prefix"
    echo "    -u update cmake cache"
    echo "    -b build only"
    echo "    -i install binary"
    echo "    -c clean build"
    echo "    -r run binary or test, optional with args '-- [runargs]'"
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
            *) echo "未知参数: $1"; exit 1 ;;
        esac
    done

    echo "action: $action, run args: $@"
    case "X$action" in
        Xupdate) update ;;
        Xbuild) build ;;
        Xinstall) install ;;
        Xclean) run_cmd rm -rf build ;;
        Xrun) run $@ ;;
        Xtest) test $@ ;;
        *) ;;
    esac
}

main $*
