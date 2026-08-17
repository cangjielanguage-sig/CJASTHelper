<#
.SYNOPSIS
    CJASTHelper Windows 构建环境路径配置（每台机器 / 每个环境一份）

.DESCRIPTION
    集中维护构建脚本中依赖本机环境（SDK / 工具链 / 安装目录）的路径，
    使 build.ps1 中不出现硬编码路径。本文件不被直接执行，由 build.ps1
    dot-source 加载：

        . "$PSScriptRoot/scripts/win_env.ps1"

    路径解析优先级：已有的同名环境变量 > 下方默认值。
    如需在不动本文件的情况下临时切换（CI、换机器、多版本 SDK），
    预先设置环境变量即可，例如：

        $env:MINGW_BIN          = "D:/sdks/llvm-mingw-15/bin"
        $env:SCOOP_SHIMS        = "C:/Users/<name>/scoop/shims"
        $env:GTEST_RELEASE_PATH = "D:/sdks/googletest"

    所有路径使用正斜杠，与 CMakeLists.txt 中的写法保持一致。
    变量以 $global: 形式导出，与 build.ps1 现有引用方式一致。

.NOTES
    修改本文件不需要改动 build.ps1；路径不存在时会给出指向本文件的警告。
#>

# ------------------------------------------------------------------------------
# MinGW / LLVM 工具链（CMakeLists.txt 中 -DMINGW_BIN 与编译器路径引用）
# ------------------------------------------------------------------------------
if ($env:MINGW_BIN) {
    $global:MINGW_BIN = $env:MINGW_BIN
} else {
    $global:MINGW_BIN = "D:/sdks/llvm-mingw-20220906-msvcrt-x86_64/bin"
}

# ------------------------------------------------------------------------------
# Cangjie SDK（与 CMakeLists.txt 顶部 CANGJIE_ROOT 对应，注意与 CANGJIE_HOME
# ------------------------------------------------------------------------------
if ($env:CANGJIE_HOME) {
} else {
    $env:CANGJIE_HOME = "D:/sdks/cangjie"
}

# ------------------------------------------------------------------------------
# Scoop shims 目录（ninja 由 Scoop 安装，追加到 PATH）
# ------------------------------------------------------------------------------
if ($env:SCOOP_SHIMS) {
} else {
    # Scoop shims: ninja
    $env:PATH += ";C:/Users/Administrator/scoop/shims"
}

# ------------------------------------------------------------------------------
# googletest 安装目录（CMakeLists 通过 ENV{GTEST_RELEASE_PATH} 读取，仅 -EnableTest 需要）
# ------------------------------------------------------------------------------
if ($env:GTEST_RELEASE_PATH) {
} else {
    $env:GTEST_RELEASE_PATH = "D:/sdks/googletest"
}

# ------------------------------------------------------------------------------
# 轻量自检：必选 SDK/工具链路径缺失时给出提示，指向本配置文件而不是 build.ps1
# ------------------------------------------------------------------------------
foreach ($p in @($global:MINGW_BIN, $env:GTEST_RELEASE_PATH)) {
    if (-not (Test-Path $p)) {
        Write-Warning "Path not found: $p (check scripts/win_env.ps1 or override via env vars)"
    }
}