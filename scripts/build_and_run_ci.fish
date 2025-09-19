# Build and run CI
set env_file $argv[1]
if test -f $env_file
    . $env_file
end
# 配置环境
cjahenv release

# 构建
bah -a -d $CANGJIE_LIB -v -t Release -g -b

# 执行
rahci
