# source cangjie

# help:
#    bash build.sh -b -r -u [run_args]

CJ_INC=$CANGJIE_HOME/include
BTYPE=Debug
BUILD_DIR=build
SOURCE_DIR=.
NINJA_BIN=ninja
VERBOSE=
EXT=.exe
CJH=$BUILD_DIR/bin/cjah$EXT
PRE=$PWD/output


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
    run_cmd cmake -G "Ninja" -B $BUILD_DIR -S $SOURCE_DIR -DCANGJIE_INCLUDE=$CJ_INC -DCMAKE_BUILD_TYPE=$BTYPE -DCMAKE_INSTALL_PREFIX=$PRE
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

function main() {
    TEMP=$(getopt -o "vp:t:ubicr" -n "opts" -- "$@")
    eval set -- "$TEMP"
    run_flag=
    while true; do
        case "$1" in
            -v)
                VERBOSE="-v"
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
                run_flag="1"
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

    if [ -n "$run_flag" ]; then
        run $@
    fi
}

main $*
