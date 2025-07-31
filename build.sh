# source cangjie

# help:
#    bash build.sh -b -r -u [run_args]
CWD=$(dirname $(realpath "$0"))
CJ_INC=$CANGJIE_SRC_HOME/include
BTYPE=Debug
BUILD_DIR=$CWD/build
SOURCE_DIR=$CWD
NINJA_BIN=ninja
VERBOSE=
EXT=
CJH=$BUILD_DIR/bin/cjah$EXT
TEST_RUNNER=$BUILD_DIR/bin/runner$EXT
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

function main() {
    TEMP=$(getopt -o "vgp:t:ubicr" -n "opts" -- "$@")
    eval set -- "$TEMP"
    run_flag=
    while true; do
        case "$1" in
            -v)
                VERBOSE="-v"
                shift
                ;;
            -g)
                TEST="ON"
                shift
                ;;
            -p)
                PRE=$2
                shift 2
                ;; 
            -t)
                BTYPE=$2
                shift 2
                ;;
            -u)
                update
                shift
                ;;
            -b)
                build
                shift
                ;;
            -i)
                install
                shift
                ;;
            -c)
                run_cmd rm -rf build
                shift
                ;;
            -r)
                run_flag="run"
                if [[ "$TEST" == "ON" ]]; then
                    run_flag="test"
                fi
                shift
                ;;
            --)
                shift
                break
                ;;
            *)
                echo "Internal error!"
                exit 1
                ;;
        esac
    done

    if [[ "X$run_flag" == "Xrun" ]]; then
        run $@
    elif [[ "X$run_flag" == "Xtest" ]]; then
        test $@
    fi
}

main $*
