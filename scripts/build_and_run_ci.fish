# Build and run CI

if test -f ~/work/scripts/env.fish
    . ~/work/scripts/env.fish
end
# 配置环境
cjahenv "release"

# 构建
bahsa -v -t Relase -g -b

# 执行
rahci
