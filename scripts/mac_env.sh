# 仓颉二进制 SDK 路径 (build.sh 依赖 $CANGJIE_HOME/tools/lib)
# dev 分支只适配 B120RC3（v1.2.0-beta.rc3）：AST::Node::ty 为私有 + GetTy() 访问器
export CANGJIE_HOME=${CJAH_SDK_ROOT:-/Users/tiny/work/dev/sdks/cangjie}/B120RC3/cangjie
# 仓颉源码路径
export CANGJIE_SRC_HOME=${CJAH_SDK_ROOT:-/Users/tiny/work/dev/sdks/cangjie}/B120RC3/cangjie_compiler-v1.2.0-beta.rc3
export GTEST_RELEASE_PATH=${GTEST_RELEASE_PATH:-/Users/tiny/work/dev/registry/cpp/googletest}
export PATH=/Users/tiny/work/dev/registry/cpp/llvm15.0.4/llvm/bin:$PATH
