<#
.SYNOPSIS
    CJASTHelper Build System for Windows with MinGW
.DESCRIPTION
    Build script for CJASTHelper using CMake, Ninja, and MinGW clang++ on Windows
#>

# Use $args directly to avoid PowerShell's built-in parameter conflicts
$rawArgs = $args

# Colors for output
$RED = [ConsoleColor]::Red
$GREEN = [ConsoleColor]::Green
$YELLOW = [ConsoleColor]::Yellow
$BLUE = [ConsoleColor]::Blue
$PURPLE = [ConsoleColor]::Magenta
$CYAN = [ConsoleColor]::Cyan
$NC = [ConsoleColor]::Gray

# ==============================================================================
# Toolchain Configuration (must be defined before use)
# ==============================================================================

# 机器/环境相关路径（SDK、工具链、安装目录）统一在 scripts/win_env.ps1 中配置，
# 支持同名环境变量覆盖，本文件不再出现硬编码路径。
. "$PSScriptRoot/scripts/win_env.ps1"

# 仓库内相对路径（不依赖外部环境，仅依赖本仓库布局）
$global:JSON_INC = "$PSScriptRoot/third_party/json/json"

# Add MinGW to PATH for ninja
# $env:PATH = "$global:MINGW_BIN;" + $env:PATH

# ==============================================================================
# Helper Functions (must be defined before use)
# ==============================================================================

function Write-Header {
    param([string]$Message)
    Write-Host -ForegroundColor $CYAN "=========================================="
    Write-Host -ForegroundColor $CYAN "     CJASTHelper Build System"
    Write-Host -ForegroundColor $CYAN "=========================================="
    if ($Message) { Write-Host -ForegroundColor $CYAN $Message }
}

function Write-Info {
    param([string]$Message)
    Write-Host -ForegroundColor $BLUE "[INFO] $Message"
}

function Write-Success {
    param([string]$Message)
    Write-Host -ForegroundColor $GREEN "[SUCCESS] $Message"
}

function Write-Warning {
    param([string]$Message)
    Write-Host -ForegroundColor $YELLOW "[WARNING] $Message"
}

function Write-Error {
    param([string]$Message)
    Write-Host -ForegroundColor $RED "[ERROR] $Message"
}

function Run-ExternalCommand {
    param(
        [string]$Cmd,
        [string[]]$CmdArgs
    )
    if ([string]::IsNullOrEmpty($Cmd)) {
        Write-Error "No command provided"
        return 1
    }
    $fullCmd = @($Cmd) + $CmdArgs
    if ($global:ShowCmd) {
        Write-Host -ForegroundColor $PURPLE "[CMD] $($fullCmd -join ' ')"
    }
    $exitCode = & $Cmd @CmdArgs
    return $LASTEXITCODE
}

function Show-Help {
    Write-Host -ForegroundColor $CYAN "Usage:"
    Write-Host "    powershell -File build.ps1 [options] -- [runargs]"
    Write-Host ""
    Write-Host -ForegroundColor $CYAN "Examples:"
    Write-Host "    powershell -File build.ps1 -ShowCmd -BuildType Debug -BuildOnly"
    Write-Host "    powershell -File build.ps1 -BuildType Release -BuildOnly"
    Write-Host "    powershell -File build.ps1 -ShowCmd -Run -- test/main.cj"
    Write-Host ""
    Write-Host -ForegroundColor $CYAN "Options:"
    Write-Host -ForegroundColor $GREEN "    --help         Show this help message"
    Write-Host -ForegroundColor $GREEN "    -ShowCmd       Dump build commands"
    Write-Host -ForegroundColor $GREEN "    -EnableTest    Enable test with googletest"
    Write-Host -ForegroundColor $GREEN "    -Standalone    Enable standalone version"
    Write-Host -ForegroundColor $GREEN "    -BuildType     Config build type [Debug | Release] (default: Release)"
    Write-Host -ForegroundColor $GREEN "    -Prefix        Config install prefix (default: output)"
    Write-Host -ForegroundColor $GREEN "    -CangjieLib    Override Cangjie lib path"
    Write-Host -ForegroundColor $GREEN "    -UpdateCache   Update cmake cache"
    Write-Host -ForegroundColor $GREEN "    -BuildOnly     Build only"
    Write-Host -ForegroundColor $GREEN "    -Install       Install binary"
    Write-Host -ForegroundColor $GREEN "    -Clean         Clean build"
    Write-Host -ForegroundColor $GREEN "    -Run           Run binary or test, optional with args '-- [runargs]'"
}

function Update-Cache {
    Write-Info "Updating CMake cache..."
    Write-Info "Cangjie include: $global:CANGJIE_INCLUDE_DIR"
    Write-Info "Cangjie lib: $global:CANGJIE_LIB_DIR"
    Write-Info "Cangjie cjnative lib: $global:CANGJIE_CJNATIVE_LIB_DIR"
    Write-Info "JSON include: $global:JSON_INC"
    Write-Info "MinGW: $global:MINGW_BIN"
    
    $cmdArray = @(
        "-G", "Ninja"
        "-B", $global:BUILD_DIR
        "-S", $global:SOURCE_DIR
        "-DJSON_INCLUDE=$JSON_INC"
        "-DCANGJIE_INCLUDE=$CANGJIE_INCLUDE_DIR"
        "-DCMAKE_BUILD_TYPE=$global:BuildType"
        "-DCMAKE_INSTALL_PREFIX=$global:PRE"
        "-DCMAKE_ENABLE_TEST=$global:TEST"
        "-DCJAH_STANDALONE=$global:ALONE"
        "-DMINGW_BIN=$global:MINGW_BIN"
        "-DCMAKE_ENABLE_ASSERT=OFF"
    )
    
    $exitCode = Run-ExternalCommand "cmake" $cmdArray
    
    if ($exitCode -eq 0) {
        Write-Success "CMake cache updated successfully"
        return 0
    } else {
        Write-Error "Failed to update CMake cache"
        return 1
    }
}

function Build-Project {
    Write-Header
    Write-Info "Building $global:BuildType version..."
    
    # Update cmake
    $exitCode = Update-Cache
    if ($exitCode -ne 0) {
        return 1
    }
    
    Write-Info "Starting build process..."
    if (-not (Test-Path $global:BUILD_DIR)) {
        New-Item -ItemType Directory -Path $global:BUILD_DIR | Out-Null
    }
    $originalDir = Get-Location
    Set-Location $global:BUILD_DIR
    $exitCode = Run-ExternalCommand $global:NINJA_BIN @($global:VERBOSE_FLAG)
    Set-Location $originalDir
    
    if ($exitCode -eq 0) {
        Write-Success "Build completed successfully"
        Copy-SdkRuntimeDeps -TargetDir "$global:BUILD_DIR/bin"
        Deploy-Config -TargetDir "$global:BUILD_DIR/bin"
        Write-Info "Binaries in: $global:BUILD_DIR\bin\"
        Write-Info "Libraries in: $global:BUILD_DIR\lib\"
    } else {
        Write-Error "Build failed"
    }
    return $exitCode
}

function Install-Project {
    Write-Header
    Write-Info "Installing binaries..."
    
    if (-not (Test-Path $global:CJAH)) {
        Write-Warning "Binary not found, building first..."
        $exitCode = Build-Project
        if ($exitCode -ne 0) {
            return 1
        }
    }
    
    $originalDir = Get-Location
    Set-Location $global:BUILD_DIR
    $exitCode = Run-ExternalCommand $global:NINJA_BIN @($global:VERBOSE_FLAG, "install")
    Set-Location $originalDir
    
    if ($exitCode -eq 0) {
        Write-Success "Installation completed successfully"
        Copy-SdkRuntimeDeps -TargetDir "$global:PRE/bin"
        Deploy-Config -TargetDir "$global:PRE/bin"
    } else {
        Write-Error "Installation failed"
    }
    return $exitCode
}

function Run-Binary {
    param([string[]]$Args)
    
    if (-not (Test-Path $global:CJAH)) {
        Write-Warning "Binary not found, building first..."
        $exitCode = Build-Project
        if ($exitCode -ne 0) {
            return 1
        }
    }
    
    if (Test-Path $global:CJAH) {
        Write-Info "Running cjah..."
        $exitCode = Run-ExternalCommand $global:CJAH @Args
        return $exitCode
    }
    return 1
}

function Run-Tests {
    param([string[]]$Args)
    
    Write-Header
    Write-Info "Running tests..."
    
    if (-not (Test-Path $global:TEST_RUNNER)) {
        Write-Warning "Test runner not found, building first..."
        $exitCode = Build-Project
        if ($exitCode -ne 0) {
            return 1
        }
    }
    
    $exitCode = Run-ExternalCommand $global:TEST_RUNNER @Args
    return $exitCode
}

# ==============================================================================
# Configuration
# ==============================================================================

# Get script directory
$CWD = $PSScriptRoot
$global:BUILD_DIR = "$CWD/build"
$global:SOURCE_DIR = $CWD
$global:NINJA_BIN = "ninja"
$global:EXT = ".exe"
$global:CJAH = "$global:BUILD_DIR/bin/cjah$global:EXT"
$global:TEST_RUNNER = "$global:BUILD_DIR/bin/cjah_test$global:EXT"
$global:PRE = "$CWD/output"

# Default configuration
$global:ShowCmd = $false
$global:EnableTest = $false
$global:Standalone = $false
$global:BuildType = "Release"
$global:Prefix = "output"
$global:CangjieLib = ""
$global:action = ""
$global:runArgs = @()
$global:parsingArgs = $true

$global:VERBOSE_FLAG = if ($global:ShowCmd) { "-v" } else { "" }
$global:TEST = if ($global:EnableTest) { "ON" } else { "OFF" }
$global:ALONE = if ($global:Standalone) { "ON" } else { "OFF" }

# ==============================================================================
# Parse Arguments
# ==============================================================================

$argIndex = 0
while ($argIndex -lt $rawArgs.Count) {
    $arg = $rawArgs[$argIndex]
    
    if ($global:parsingArgs -and $arg -eq '--') {
        $global:parsingArgs = $false
        $argIndex++
        continue
    }
    
    if ($global:parsingArgs) {
        switch -Regex ($arg) {
            '^--?[Hh]elp$' { Show-Help; exit 0 }
            '^-ShowCmd$' { $global:ShowCmd = $true; $global:VERBOSE_FLAG = "-v" }
            '^-EnableTest$' { $global:EnableTest = $true; $global:TEST = "ON" }
            '^-Standalone$' { $global:Standalone = $true; $global:ALONE = "ON" }
            '^-BuildType$' { 
                if ($argIndex + 1 -lt $rawArgs.Count) {
                    $global:BuildType = $rawArgs[$argIndex + 1]
                    $argIndex++
                }
            }
            '^-Prefix$' { 
                if ($argIndex + 1 -lt $rawArgs.Count) {
                    $global:Prefix = $rawArgs[$argIndex + 1]
                    $global:PRE = "$CWD/$global:Prefix"
                    $argIndex++
                }
            }
            '^-CangjieLib$' { 
                if ($argIndex + 1 -lt $rawArgs.Count) {
                    $global:CangjieLib = $rawArgs[$argIndex + 1]
                    $global:CANGJIE_LIB_DIR = $global:CangjieLib
                    $argIndex++
                }
            }
            '^-UpdateCache$' { $global:action = "update" }
            '^-BuildOnly$' { $global:action = "build" }
            '^-Install$' { $global:action = "install" }
            '^-Clean$' { $global:action = "clean" }
            '^-Run$' { 
                $global:action = if ($global:EnableTest) { "test" } else { "run" }
            }
            default { Write-Error "Unknown parameter: $arg"; exit 1 }
        }
    } else {
        $global:runArgs += $arg
    }
    $argIndex++
}

Write-Info "Action: $global:action, Run args: $global:runArgs"

# ==============================================================================
# Unify Cangjie SDK (must be before cmake/run)
# ==============================================================================
# CANGJIE_HOME 必须与编译头文件同源。若用户环境中 CANGJIE_HOME 指向其他 SDK
# （如 .cangjie-sdk/6.1），会与 win_env.ps1 配置的 SDK 头文件混用：编译按一套
# 类布局构造对象，运行时却加载另一版本 libcangjie-lsp.dll，跨 DLL 访问即
# 段错误（SIGSEGV）。此处强制统一，并让 CMakeLists 的 IMPORTED_IMPLIB
# 和运行时 DLL 搜索都落到同一 SDK。
if ($global:CangjieLib) {
    $env:CANGJIE_HOME = $global:CangjieLib
}
Write-Info "CANGJIE_HOME (unified): $env:CANGJIE_HOME"
$global:CANGJIE_INCLUDE_DIR     = "$env:CANGJIE_HOME/include"
$global:CANGJIE_LIB_DIR         = "$env:CANGJIE_HOME/tools/lib"
$global:CANGJIE_CJNATIVE_LIB_DIR = "$env:CANGJIE_HOME/lib/windows_x86_64_cjnative"

# ==============================================================================
# Runtime deployment helpers
# ==============================================================================

# 将 SDK 运行时依赖 DLL 复制到目标 bin 目录（exe 所在目录 DLL 搜索优先级最高，
# 不依赖 PATH 中其它 SDK 的干扰）
function Copy-SdkRuntimeDeps {
    param([string]$TargetDir)
    $sdkBin = Join-Path $env:CANGJIE_HOME "tools/bin"
    if (-not (Test-Path $sdkBin)) {
        Write-Warning "SDK bin dir not found: $sdkBin"
        return
    }
    $deps = @("libcangjie-lsp.dll", "libc++.dll", "libunwind.dll", "libwinpthread-1.dll")
    foreach ($d in $deps) {
        $src = Join-Path $sdkBin $d
        if (Test-Path $src) {
            Copy-Item -Force $src $TargetDir
            Write-Info "Deployed $d -> $TargetDir"
        } else {
            Write-Warning "SDK dep not found: $src"
        }
    }
}

# 将 config/*.json 部署到目标 bin/config（FindPath 的第二个候选位置，稳定命中）
function Deploy-Config {
    param([string]$TargetDir)
    $cfgSrc = Join-Path $PSScriptRoot "config"
    if (Test-Path $cfgSrc) {
        $cfgDst = Join-Path $TargetDir "config"
        New-Item -ItemType Directory -Force $cfgDst | Out-Null
        Copy-Item -Force "$cfgSrc/*.json" $cfgDst
        Write-Info "Deployed config -> $cfgDst"
    }
}

# ==============================================================================
# Dispatch Action
# ==============================================================================

switch ($global:action) {
    "update" { exit Update-Cache }
    "build" { exit Build-Project }
    "install" { exit Install-Project }
    "clean" {
        Write-Info "Cleaning build directory..."
        if (Test-Path "build") { Remove-Item -Recurse -Force "build" }
        if (Test-Path "output") { Remove-Item -Recurse -Force "output" }
        Write-Success "Clean completed"
    }
    "run" { exit Run-Binary $global:runArgs }
    "test" { exit Run-Tests $global:runArgs }
    default {
        if (-not $global:action) {
            Write-Header
            Write-Info "No action specified. Use --help for help."
        }
    }
}