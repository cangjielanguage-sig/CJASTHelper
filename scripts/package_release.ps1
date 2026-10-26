<#
.SYNOPSIS
    CJASTHelper 发布打包脚本（Windows 版，对应 scripts/package_release.sh）
.DESCRIPTION
    把 `powershell -File build.ps1 -a -i` 产出的安装目录按版本号打成可分发的 zip：

        dist\cjah_<version>.zip          完整版（bin\ config\ lib\，含 pass 插件）
        dist\cjah_minimal_<version>.zip  最小版（仅语法检查：裁掉插件，选项只留 check-syntax）

    参数、目录布局、自检用例与 package_release.sh 一一对应，平台差异只出现在
    「归档格式」「依赖闭环检查工具」「清环境方式」三处：

        环节            Linux/macOS (sh)              Windows (本脚本)
        --------------  ----------------------------  -------------------------------------
        归档            tar.gz                        zip（System32\tar.exe 生成，条目用 '/'）
        依赖闭环        ldd / otool -L                PE 导入表（llvm-objdump / objdump）
        独立运行自检    env -i 清空环境               ProcessStartInfo 清空全部环境变量
        只读目录用例    chmod 555 当前目录            跳过（Windows 无 chmod 语义）

    独立运行自检的判定依据：Windows 加载器只按「exe 目录 -> 系统目录 -> PATH」找 DLL，
    脚本把子进程环境变量全部清空（只留 SystemRoot/SystemDrive/TEMP/TMP），因此
    自检通过即证明产物不依赖 CANGJIE_HOME / PATH / MINGW_BIN 等任何环境配置。

用法:
    powershell -ExecutionPolicy Bypass -File scripts\package_release.ps1 -v alpha_0.0.7
    powershell -File scripts\package_release.ps1 -v alpha_0.0.7 --minimal

选项:
    -v, --version <ver>  版本号（必填），如 alpha_0.0.7
    -m, --minimal        生成最小版（只支持 --check-syntax，裁掉 pass 插件）
    -p, --prefix <dir>   安装目录（默认 <repo>\output，由 build.ps1 -a -i 产出）
    -o, --out <dir>      打包输出目录（默认 <repo>\dist）
    -k, --keep           保留暂存目录（默认打完成包后删除）
    -h, --help           显示帮助

依赖:
    PowerShell 5.1+；tar.exe（Win10 1803+ 自带，缺失时回退 Compress-Archive）；
    objdump / llvm-objdump（可选：静态依赖分析，缺失时仅跳过静态检查，运行时自检照做）
#>

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

# 输出被重定向（管道 / CI / 写日志）时改用 UTF-8，否则中文会按控制台代码页（cp936）
# 编码进管道，在 bash / CI 日志里变乱码；交互式控制台保持系统默认，与 build.ps1 一致。
if ([Console]::IsOutputRedirected) {
    try { [Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false) } catch { }
}

# ==============================================================================
# 基础输出
# ==============================================================================
$script:RawArgs = $args
$script:Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

function Write-Info {
    param([string]$Message)
    Write-Host "[INFO] $Message" -ForegroundColor Blue
}

function Write-Ok {
    param([string]$Message)
    Write-Host "[ OK ] $Message" -ForegroundColor Green
}

function Write-Warn {
    param([string]$Message)
    Write-Host "[WARN] $Message" -ForegroundColor Yellow
}

function Write-Fail {
    param([string]$Message)
    Write-Host "[FAIL] $Message" -ForegroundColor Red
}

function Die {
    param([string]$Message)
    Write-Fail $Message
    exit 1
}

# 打印文件头注释（等价 package_release.sh 的 `sed -n '2,24p'`）
function Show-Help {
    $inside = $false
    foreach ($line in (Get-Content -LiteralPath $PSCommandPath -Encoding UTF8)) {
        if ($line -match '^<#') { $inside = $true; continue }
        if ($line -match '^#>') { break }
        if (-not $inside) { continue }
        $text = $line -replace '^#{1,2} ?', ''
        # 去掉 `.SYNOPSIS` 这类标签的前导点，便于阅读
        Write-Host ($text -replace '^\.(?=[A-Z]+$)', '')
    }
}

# ==============================================================================
# 参数解析（用 $args 手工解析：脚本一旦带 param() 就变成 advanced script，
# -out / -prefix 之类会与公共参数 -OutVariable 等前缀歧义）
# ==============================================================================
$script:Version = ''
$script:Minimal = $false
$script:Prefix = Join-Path $script:Root 'output'
$script:OutDir = Join-Path $script:Root 'dist'
$script:Keep = $false

# 取选项后面的取值（越界时给出明确报错，而不是把空串当成版本号）
function Get-OptionValue {
    param([int]$Index, [string]$Flag)
    if (($Index + 1) -ge $script:RawArgs.Count) {
        Die "$Flag 缺少参数（-h 查看用法）"
    }
    return [string]$script:RawArgs[$Index + 1]
}

function Parse-Args {
    $i = 0
    while ($i -lt $script:RawArgs.Count) {
        $arg = [string]$script:RawArgs[$i]
        switch ($arg) {
            '-v' { $script:Version = Get-OptionValue -Index $i -Flag '-v'; $i += 2; continue }
            '--version' { $script:Version = Get-OptionValue -Index $i -Flag '--version'; $i += 2; continue }
            '-m' { $script:Minimal = $true; $i++; continue }
            '--minimal' { $script:Minimal = $true; $i++; continue }
            '-p' { $script:Prefix = Get-OptionValue -Index $i -Flag '-p'; $i += 2; continue }
            '--prefix' { $script:Prefix = Get-OptionValue -Index $i -Flag '--prefix'; $i += 2; continue }
            '-o' { $script:OutDir = Get-OptionValue -Index $i -Flag '-o'; $i += 2; continue }
            '--out' { $script:OutDir = Get-OptionValue -Index $i -Flag '--out'; $i += 2; continue }
            '-k' { $script:Keep = $true; $i++; continue }
            '--keep' { $script:Keep = $true; $i++; continue }
            '-h' { Show-Help; exit 0 }
            '--help' { Show-Help; exit 0 }
            default {
                Die "未知参数: $arg（-h 查看用法）"
            }
        }
    }

    if ([string]::IsNullOrWhiteSpace($script:Version)) {
        Die "必须用 -v/--version 指定版本号，如 -v alpha_0.0.7"
    }
    # 版本号只允许出现在目录/文件名里：字母数字与 . _ -
    if ($script:Version -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]*$') {
        Die "版本号 '$script:Version' 非法：仅允许字母、数字、'.'、'_'、'-'，且不能以符号开头"
    }

    # 相对路径按调用目录解析，绝对路径直接用
    if (-not [System.IO.Path]::IsPathRooted($script:Prefix)) {
        $script:Prefix = Join-Path (Get-Location).Path $script:Prefix
    }
    if (-not [System.IO.Path]::IsPathRooted($script:OutDir)) {
        $script:OutDir = Join-Path (Get-Location).Path $script:OutDir
    }
    $script:Prefix = [System.IO.Path]::GetFullPath($script:Prefix)
    $script:OutDir = [System.IO.Path]::GetFullPath($script:OutDir)
}

# ==============================================================================
# 安装目录检查
# ==============================================================================
function Test-Prefix {
    if (-not (Test-Path -LiteralPath $script:Prefix)) {
        Die "安装目录不存在: $script:Prefix（先执行 powershell -File build.ps1 -a -i）"
    }
    $exe = Join-Path $script:Prefix 'bin\cjah.exe'
    if (-not (Test-Path -LiteralPath $exe)) {
        Die "未找到可执行文件: $exe（先执行 powershell -File build.ps1 -a -i）"
    }
    # alone 模式的判据：SDK 前端 DLL 必须已随包落在 bin\ 下（build.ps1 -a 负责部署）。
    # 先查这一项：指向 build\ 等非安装目录时，这条提示比「缺 config」更可操作。
    $lsp = Get-ChildItem -LiteralPath (Join-Path $script:Prefix 'bin') -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like '*cangjie-lsp.dll' }
    if (-not $lsp) {
        Die "安装目录缺少 libcangjie-lsp.dll（不是 alone 安装产物，打包后无法脱离 SDK 运行），请用 powershell -File build.ps1 -a -i 重新安装"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $script:Prefix 'config'))) {
        Die "安装目录缺少 config\（安装前缀应为 build.ps1 -a -i 的产物，默认 output）: $(Join-Path $script:Prefix 'config')"
    }
    Write-Ok "安装目录检查通过: $script:Prefix"
}

# ==============================================================================
# PE 依赖分析（静态闭环检查）
# ==============================================================================
function Find-Objdump {
    $candidates = @()
    if ($env:MINGW_BIN) {
        $candidates += (Join-Path $env:MINGW_BIN 'llvm-objdump.exe')
    }
    foreach ($name in @('llvm-objdump', 'objdump')) {
        $cmd = Get-Command $name -ErrorAction SilentlyContinue
        if ($cmd) { $candidates += $cmd.Source }
    }
    foreach ($c in $candidates) {
        if ($c -and (Test-Path -LiteralPath $c)) { return $c }
    }
    return $null
}

# 读取 PE 导入表里的 DLL 名（llvm-objdump / objdump 的 "DLL Name: xxx" 行）
function Get-PeImports {
    param([string]$File, [string]$Objdump)

    $names = @()
    foreach ($line in (& $Objdump -p $File 2>$null)) {
        # 用 -cmatch 区分导入行 "DLL Name:" 与导出表的 "DLL name:"
        if ($line -cmatch '^\s*DLL Name:\s*(\S+)\s*$') { $names += $Matches[1] }
    }
    return ($names | Sort-Object -Unique)
}

# 依赖闭环自检：bin\ 下每个 exe/dll 的非系统导入都必须能在包内 bin\ 解析
function Test-Closure {
    param([string]$Dir, [string]$Objdump)

    $binDir = Join-Path $Dir 'bin'
    if (-not $Objdump) {
        Write-Warn "未找到 objdump/llvm-objdump，跳过静态依赖分析（运行时自检仍会证明依赖闭环）"
        return $true
    }

    $packaged = @{}
    foreach ($f in (Get-ChildItem -LiteralPath $binDir -File)) {
        if ($f.Extension -eq '.dll') { $packaged[$f.Name.ToLower()] = $true }
    }

    $missing = @()
    foreach ($t in (Get-ChildItem -LiteralPath $binDir -File)) {
        if ($t.Extension -notin @('.exe', '.dll')) { continue }
        foreach ($dep in (Get-PeImports -File $t.FullName -Objdump $Objdump)) {
            $lower = $dep.ToLower()
            if ($packaged.ContainsKey($lower)) { continue }
            if ($lower -like 'api-ms-win-*' -or $lower -like 'ext-ms-win-*') { continue }
            if (Test-Path -LiteralPath (Join-Path $env:SystemRoot "System32\$dep")) { continue }
            $missing += ("{0} -> {1}" -f $t.Name, $dep)
        }
    }

    if ($missing.Count -gt 0) {
        Write-Fail "存在包外依赖（打包后无法脱离环境运行）:"
        foreach ($m in $missing) { Write-Host "       $m" -ForegroundColor Red }
        return $false
    }
    Write-Ok "自检: 依赖闭环（PE 导入表全部命中包内 bin\ 或系统目录）"
    return $true
}

# ==============================================================================
# 清环境运行（独立使用验证）
# ==============================================================================
# Windows 命令行参数转义（.NET Framework 4.x 的 ProcessStartInfo 没有 ArgumentList）
function Quote-Arg {
    param([string]$Argument)

    if ($Argument -ne '' -and $Argument -notmatch '[\s"]') { return $Argument }
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.Append('"')
    $backslashes = 0
    foreach ($ch in $Argument.ToCharArray()) {
        if ($ch -eq '\') { $backslashes++; continue }
        if ($ch -eq '"') {
            [void]$sb.Append('\' * (2 * $backslashes + 1))
            [void]$sb.Append('"')
            $backslashes = 0
            continue
        }
        if ($backslashes -gt 0) { [void]$sb.Append('\' * $backslashes); $backslashes = 0 }
        [void]$sb.Append($ch)
    }
    if ($backslashes -gt 0) { [void]$sb.Append('\' * (2 * $backslashes)) }
    [void]$sb.Append('"')
    return $sb.ToString()
}

# 在「清空所有环境变量」的子进程里跑 cjah（只保留 Windows 进程启动必需项与临时目录），
# 等价于 sh 版的 env -i。
function Invoke-Clean {
    param(
        [Parameter(Mandatory = $true)][string]$Exe,
        [string[]]$Argv = @(),
        [Parameter(Mandatory = $true)][string]$WorkDir,
        [string]$TempDir,
        [hashtable]$ExtraEnv
    )

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $Exe
    $psi.Arguments = (($Argv | ForEach-Object { Quote-Arg $_ }) -join ' ')
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.WorkingDirectory = $WorkDir

    $psi.EnvironmentVariables.Clear()
    $psi.EnvironmentVariables['SystemRoot'] = $env:SystemRoot
    $psi.EnvironmentVariables['SystemDrive'] = $env:SystemDrive
    if (-not $TempDir) { $TempDir = $WorkDir }
    $psi.EnvironmentVariables['TEMP'] = $TempDir
    $psi.EnvironmentVariables['TMP'] = $TempDir
    if ($ExtraEnv) {
        foreach ($k in $ExtraEnv.Keys) { $psi.EnvironmentVariables[$k] = $ExtraEnv[$k] }
    }

    $proc = [System.Diagnostics.Process]::Start($psi)
    $outTask = $proc.StandardOutput.ReadToEndAsync()
    $errTask = $proc.StandardError.ReadToEndAsync()
    $proc.WaitForExit()

    return [pscustomobject]@{
        ExitCode = $proc.ExitCode
        Out      = $outTask.Result
        Err      = $errTask.Result
    }
}

# ==============================================================================
# 最小版裁剪
# ==============================================================================
function Set-MinimalLayout {
    param([string]$Dir)

    # 1) pass 插件（check-syntax 只跑 parse 阶段，不走任何 pass）
    #    Windows(MinGW) 去掉了 lib 前缀、装到 bin\（cjast_*_pass.dll）
    $removed = @()
    foreach ($sub in @('bin', 'lib')) {
        $d = Join-Path $Dir $sub
        if (-not (Test-Path -LiteralPath $d)) { continue }
        Get-ChildItem -LiteralPath $d -File | Where-Object {
            $_.Name -like 'cjast_*_pass.dll' -or $_.Name -like 'libcjast_*_pass.*'
        } | ForEach-Object {
            Remove-Item -LiteralPath $_.FullName -Force
            $removed += $_.Name
        }
    }
    if ($removed.Count -gt 0) {
        Write-Info "裁剪插件: $($removed -join ', ')"
    }
    $dlls = (Get-ChildItem -LiteralPath (Join-Path $Dir 'bin') -File | Where-Object { $_.Extension -eq '.dll' } |
        ForEach-Object { $_.Name }) -join ' '
    Write-Info "保留动态库: $dlls"

    # 2) 与 check-syntax 无关的配置一并裁掉：
    #    tasks.json / stdcore_sema_task.json 是任务配置；passes.json 是 pass 配置
    #    —— check-syntax 不跑任何 pass，启动时也不加载它
    #    （ArgHelper::ConfigGlobalOptions 在 check-syntax 下跳过 PassManager::Init），
    #    因此最小版 config\ 只留 valid_options.json。
    foreach ($name in @('tasks.json', 'stdcore_sema_task.json', 'passes.json')) {
        $p = Join-Path (Join-Path $Dir 'config') $name
        if (Test-Path -LiteralPath $p) { Remove-Item -LiteralPath $p -Force }
    }

    # 3) valid_options.json 只保留 check-syntax（从安装目录的原始文件裁剪，避免漂移）
    $optPath = Join-Path (Join-Path $Dir 'config') 'valid_options.json'
    if (-not (Test-Path -LiteralPath $optPath)) {
        Write-Fail "最小版缺少 config\valid_options.json（启动即会失败）"
        return $false
    }

    # 注意：PowerShell 5.1 的 ConvertFrom-Json 把 JSON 数组当作「单个对象」写进管道，
    # `@(...)` 会把它包成长度为 1 的数组（再序列化就变成 {value:..., Count:...}），
    # 因此这里必须显式 foreach 展开成真正的元素数组。
    $parsed = Get-Content -LiteralPath $optPath -Raw -Encoding UTF8 | ConvertFrom-Json
    $opts = @()
    foreach ($o in $parsed) { $opts += $o }

    $keep = @($opts | Where-Object { $_.key -eq 'check-syntax' })
    if ($keep.Count -ne 1) {
        Write-Fail ("valid_options.json 中未找到唯一的 check-syntax 条目: {0}" -f (($opts | ForEach-Object { $_.key }) -join ', '))
        return $false
    }

    # 单项也写成 JSON 数组（手写外层方括号：ConvertTo-Json 对单对象不会输出数组）
    $itemJson = ConvertTo-Json -InputObject $keep[0] -Depth 10
    # 无 BOM 写入（前端 JSON 解析器不保证跳过 BOM）
    [System.IO.File]::WriteAllText($optPath, "[`r`n$itemJson`r`n]`r`n", (New-Object System.Text.UTF8Encoding($false)))

    # 复核：能被解析、且只剩一条 check-syntax
    $checkParsed = Get-Content -LiteralPath $optPath -Raw -Encoding UTF8 | ConvertFrom-Json
    $check = @()
    foreach ($c in $checkParsed) { $check += $c }
    if ($check.Count -ne 1 -or $check[0].key -ne 'check-syntax') {
        Write-Fail "裁剪后的 valid_options.json 校验失败（期望仅 check-syntax，实际 $($check.Count) 条）"
        return $false
    }
    Write-Ok "最小版: config 仅保留 valid_options.json（check-syntax）"
    return $true
}

# ==============================================================================
# 打包前自检：按实际发布形态、在清空环境的子进程里跑真实用例
# ==============================================================================
function New-TestCases {
    param([Parameter(Mandatory = $true)][string]$Dir)

    $ok = Join-Path $Dir 'ok.cj'
    $bad = Join-Path $Dir 'bad.cj'
    $multi = Join-Path $Dir 'multi'
    New-Item -ItemType Directory -Force -Path (Join-Path $multi 'pkgA') | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $multi 'pkgB') | Out-Null
    [System.IO.File]::WriteAllText($ok, "func add(a: Int64, b: Int64): Int64 {`r`n    return a + b`r`n}`r`n")
    [System.IO.File]::WriteAllText($bad, "func broken( {`r`n")
    [System.IO.File]::WriteAllText((Join-Path $multi 'pkgA\a.cj'), "package pkgA`r`nfunc a(): Unit {}`r`n")
    [System.IO.File]::WriteAllText((Join-Path $multi 'pkgB\b.cj'), "package pkgB`r`nfunc b(): Unit {}`r`n")

    return [pscustomobject]@{ Ok = $ok; Bad = $bad; Multi = $multi }
}

function Test-Package {
    param([Parameter(Mandatory = $true)][string]$Dir)

    $exe = Join-Path $Dir 'bin\cjah.exe'
    $checkSandbox = Join-Path ([System.IO.Path]::GetTempPath()) ('cjah_selfcheck_' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Force -Path $checkSandbox | Out-Null
    $failures = @()

    try {
        $cases = New-TestCases -Dir $checkSandbox
        $outDir = Join-Path $checkSandbox 'out'
        New-Item -ItemType Directory -Force -Path $outDir | Out-Null

        $run = {
            param([string[]]$Argv)
            Invoke-Clean -Exe $exe -Argv $Argv -WorkDir $checkSandbox -TempDir $checkSandbox
        }

        # 1) 语法正确的单文件：必须 0
        $r = & $run @('--check-syntax=true', $cases.Ok)
        if ($r.ExitCode -ne 0) {
            $failures += "语法正确用例返回非 0（退出码 $($r.ExitCode)）"
        } else {
            Write-Ok "自检: 单文件语法检查通过"
        }

        # 2) 语法错误的单文件：必须非 0
        $r = & $run @('--check-syntax=true', $cases.Bad)
        if ($r.ExitCode -eq 0) {
            $failures += "语法错误用例返回了 0"
        } else {
            Write-Ok "自检: 语法错误用例被正确判错（退出码非 0）"
        }

        # 3) 目录输入（多包分组）：必须 0
        $r = & $run @('--check-syntax=true', $cases.Multi)
        if ($r.ExitCode -ne 0) {
            $failures += "目录（多包）输入返回非 0（退出码 $($r.ExitCode)）"
        } else {
            Write-Ok "自检: 目录（多包）输入通过"
        }

        # 3b) 目录内含语法错误文件：必须非 0
        $multiBad = Join-Path $cases.Multi 'pkgB\bad.cj'
        [System.IO.File]::WriteAllText($multiBad, "func x( {`r`n")
        $r = & $run @('--check-syntax=true', $cases.Multi)
        Remove-Item -LiteralPath $multiBad -Force -ErrorAction SilentlyContinue
        if ($r.ExitCode -eq 0) {
            $failures += "目录内含语法错误文件却返回 0"
        } else {
            Write-Ok "自检: 目录内含语法错误被正确判错"
        }

        # 3c) `--output-dir <dir>` 两段式写法（历史 bug：取值被当成输入目录 ⇒ 假通过）
        $r = & $run @('--check-syntax=true', '--output-dir', $outDir, $cases.Bad)
        if ($r.ExitCode -eq 0) {
            $failures += "'--output-dir <dir>' 两段式写法下语法错误未被判错（假通过）"
        } else {
            Write-Ok "自检: '--output-dir <dir>' 两段式写法仍能判错"
        }

        # 3d) 只读当前目录用例：Windows 无 chmod 语义，跳过（Linux/macOS 由 sh 版覆盖）

        # 3e) 传了参数但没有任何「本发行版支持的」工具选项 ⇒ 必须非 0
        #     （历史 bug：只打印帮助就返回 0，脚本把「什么都没做」当成检查通过）
        $r = & $run @('--no-such-tool-option=true', $cases.Ok)
        if ($r.ExitCode -eq 0) {
            $failures += "未知工具选项返回了 0（只打帮助即成功 ⇒ 假通过）"
        } else {
            Write-Ok "自检: 未知工具选项被拒（退出码非 0）"
        }

        # 4) 选项可见性：最小版不得出现 check-syntax 之外的选项
        $help = & $run @('--help')
        $helpText = "$($help.Out)$($help.Err)"
        if ($helpText -notmatch '--check-syntax') {
            $failures += "--help 未列出 check-syntax"
        } else {
            Write-Ok "自检: --help 列出 check-syntax"
        }
        if ($script:Minimal) {
            # 只比对「选项清单」里的条目（行首缩进 + --key），
            # 因为 check-syntax 的描述里本身会提到 --dump-source 等其它选项名
            $extra = @($helpText -split "`r?`n" |
                ForEach-Object { if ($_ -cmatch '^\s+(--[a-z][a-z-]*)') { $Matches[1] } } |
                Where-Object { $_ -ne '--check-syntax' } |
                Sort-Object -Unique)
            if ($extra.Count -gt 0) {
                $failures += "最小版仍暴露其它选项: $($extra -join ', ')"
            } else {
                Write-Ok "自检: 选项仅剩 --check-syntax"
            }

            # 4b) 被裁掉的选项必须被拒（不能「只打帮助 + 退出 0」）
            $r = & $run @('--dump-source=parse', $cases.Ok)
            if ($r.ExitCode -eq 0) {
                $failures += "最小版对被裁掉的 --dump-source 返回 0（假通过）"
            } else {
                Write-Ok "自检: 被裁掉的选项被拒（退出码非 0）"
            }

            # 5) 插件确实已裁掉
            $leftover = @(Get-ChildItem -LiteralPath (Join-Path $Dir 'bin') -File |
                Where-Object { $_.Name -like '*cjast_*_pass.dll' })
            if ($leftover.Count -gt 0) {
                $failures += "最小版仍含 pass 插件: $(($leftover | ForEach-Object { $_.Name }) -join ', ')"
            } else {
                Write-Ok "自检: 无 pass 插件残留"
            }

            # 5b) config 只保留 valid_options.json
            $cfgExtra = @(Get-ChildItem -LiteralPath (Join-Path $Dir 'config') -File |
                Where-Object { $_.Name -ne 'valid_options.json' })
            if ($cfgExtra.Count -gt 0) {
                $failures += "最小版 config 含多余文件: $(($cfgExtra | ForEach-Object { $_.Name }) -join ', ')"
            } else {
                Write-Ok "自检: config 仅含 valid_options.json"
            }
        } else {
            # 完整版额外用例：dump-source=parse 走 pass 插件 + config\passes.json，
            # 用来证明插件在清空环境下也能被加载（最小版没有插件，不适用）
            $r = & $run @('--dump-source=parse', $cases.Ok, '--output-dir', $outDir)
            $produced = @(Get-ChildItem -LiteralPath $outDir -File -Filter '*_source.cj' -ErrorAction SilentlyContinue)
            if ($r.ExitCode -ne 0 -or $produced.Count -eq 0) {
                $failures += "dump-source=parse 失败（退出码 $($r.ExitCode)，产物 $($produced.Count) 个）"
            } else {
                Write-Ok "自检: pass 插件在清空环境下可加载（dump-source=parse 产出 $(($produced | ForEach-Object { $_.Name }) -join ', ')）"
            }
        }

        # 6) 依赖闭环（不依赖外部 SDK / PATH）
        $objdump = Find-Objdump
        if (-not (Test-Closure -Dir $Dir -Objdump $objdump)) {
            $failures += "依赖闭环检查未通过"
        }
    } finally {
        Remove-Item -LiteralPath $checkSandbox -Recurse -Force -ErrorAction SilentlyContinue
    }

    if ($failures.Count -gt 0) {
        foreach ($f in $failures) { Write-Fail "自检失败: $f" }
        return $false
    }
    return $true
}

# ==============================================================================
# 归档
# ==============================================================================
function New-Archive {
    param(
        [Parameter(Mandatory = $true)][string]$StageDir,
        [Parameter(Mandatory = $true)][string]$OutDir,
        [Parameter(Mandatory = $true)][string]$Name
    )

    $zip = Join-Path $OutDir "$Name.zip"
    if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }

    $tar = Join-Path $env:SystemRoot 'System32\tar.exe'
    if (Test-Path -LiteralPath $tar) {
        # bsdtar（Windows 自带）：-a 按扩展名选格式，zip 条目分隔符为 '/'，跨工具可解
        & $tar -a -c -f $zip -C $OutDir $Name
        if ($LASTEXITCODE -ne 0) { throw "tar.exe 打包失败（退出码 $LASTEXITCODE）" }
    } else {
        Write-Warn "未找到 tar.exe，回退 Compress-Archive（条目分隔符可能为 '\'，部分工具解压异常）"
        Compress-Archive -Path $StageDir -DestinationPath $zip -CompressionLevel Optimal
    }
    return $zip
}

function Get-Sha256 {
    param([Parameter(Mandatory = $true)][string]$Path)

    $sha = [System.Security.Cryptography.SHA256]::Create()
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $bytes = $sha.ComputeHash($stream)
    } finally {
        $stream.Dispose()
        $sha.Dispose()
    }
    return (($bytes | ForEach-Object { $_.ToString('x2') }) -join '')
}

# ==============================================================================
# 解包后验证：解到「带空格 + 中文」的临时路径，在清环境子进程里重跑关键用例
# ==============================================================================
function Test-ExtractedArchive {
    param(
        [Parameter(Mandatory = $true)][string]$ZipPath,
        [Parameter(Mandatory = $true)][string]$Name
    )

    $root = Join-Path ([System.IO.Path]::GetTempPath()) ('cjah_unpack_' + [guid]::NewGuid().ToString('N'))
    # 顺带验证：路径含空格与中文时，配置查找 / DLL 加载仍然正常
    $dest = Join-Path $root '独立 验证'
    New-Item -ItemType Directory -Force -Path $dest | Out-Null

    try {
        $tar = Join-Path $env:SystemRoot 'System32\tar.exe'
        if (Test-Path -LiteralPath $tar) {
            & $tar -x -f $ZipPath -C $dest
            if ($LASTEXITCODE -ne 0) { throw "tar.exe 解包失败（退出码 $LASTEXITCODE）" }
        } else {
            Expand-Archive -LiteralPath $ZipPath -DestinationPath $dest -Force
        }

        $exe = Join-Path $dest "$Name\bin\cjah.exe"
        if (-not (Test-Path -LiteralPath $exe)) {
            Write-Fail "解包后未找到 $Name\bin\cjah.exe"
            return $false
        }

        $work = Join-Path $root 'work'
        New-Item -ItemType Directory -Force -Path $work | Out-Null
        $cases = New-TestCases -Dir $work

        $okRun = Invoke-Clean -Exe $exe -Argv @('--check-syntax=true', $cases.Ok) -WorkDir $work -TempDir $work
        $badRun = Invoke-Clean -Exe $exe -Argv @('--check-syntax=true', $cases.Bad) -WorkDir $work -TempDir $work

        if ($okRun.ExitCode -ne 0) {
            Write-Fail "解包产物自检失败: 语法正确用例返回 $($okRun.ExitCode)"
            Write-Host $okRun.Err -ForegroundColor Red
            return $false
        }
        if ($badRun.ExitCode -eq 0) {
            Write-Fail "解包产物自检失败: 语法错误用例返回 0"
            return $false
        }

        Write-Ok "自检: 解包到含空格/中文路径并清空环境后仍可独立运行"
        return $true
    } finally {
        Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# ==============================================================================
# 主流程
# ==============================================================================
function Main {
    Parse-Args

    $name = "cjah_$($script:Version)"
    if ($script:Minimal) { $name = "cjah_minimal_$($script:Version)" }

    Test-Prefix
    New-Item -ItemType Directory -Force -Path $script:OutDir | Out-Null

    $stage = Join-Path $script:OutDir $name
    $modeText = '完整版'
    if ($script:Minimal) { $modeText = '最小版/仅语法检查' }

    Write-Info "版本号: $($script:Version)"
    Write-Info "安装目录: $($script:Prefix)"
    Write-Info "输出目录: $($script:OutDir)"
    Write-Info "包名: $name（$modeText）"

    if (Test-Path -LiteralPath $stage) {
        # 已存在的暂存目录可能被上一版遗留（如 sh 版在 Windows 上的残留），直接重建
        Remove-Item -LiteralPath $stage -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $stage | Out-Null

    foreach ($sub in @('bin', 'config')) {
        $src = Join-Path $script:Prefix $sub
        if (-not (Test-Path -LiteralPath $src)) { Die "安装目录缺少 $sub\: $src" }
        Copy-Item -LiteralPath $src -Destination $stage -Recurse -Force
    }
    # lib\ 在 Windows 上只有导入库（*.dll.a），运行时用不到，缺失不致命
    $libSrc = Join-Path $script:Prefix 'lib'
    if (Test-Path -LiteralPath $libSrc) {
        Copy-Item -LiteralPath $libSrc -Destination $stage -Recurse -Force
    } else {
        Write-Warn "安装目录缺少 lib\（Windows 上仅含导入库，不影响运行）: $libSrc"
    }

    # 随包附带版本信息与许可证（便于分发后溯源）
    [System.IO.File]::WriteAllText((Join-Path $stage 'VERSION'), "$($script:Version)`r`n",
        (New-Object System.Text.UTF8Encoding($false)))
    foreach ($f in @('LICENSE', 'README.md')) {
        $src = Join-Path $script:Root $f
        if (Test-Path -LiteralPath $src) { Copy-Item -LiteralPath $src -Destination $stage -Force }
    }

    if ($script:Minimal) {
        if (-not (Set-MinimalLayout -Dir $stage)) { Die "最小版裁剪失败" }
    }

    if (-not (Test-Package -Dir $stage)) {
        Write-Fail "自检未通过，暂存目录保留以便排查: $stage"
        exit 1
    }

    $zip = New-Archive -StageDir $stage -OutDir $script:OutDir -Name $name
    $unpackOk = Test-ExtractedArchive -ZipPath $zip -Name $name

    if (-not $script:Keep) {
        Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
    }

    if (-not $unpackOk) {
        Write-Fail "包已生成但解包验证未通过: $zip"
        exit 1
    }

    $size = (Get-Item -LiteralPath $zip).Length
    $sha = Get-Sha256 -Path $zip

    Write-Host ''
    Write-Ok "打包完成: $zip"
    Write-Host ("     大小: {0:N1} MB" -f ($size / 1MB))
    Write-Host "     sha256: $sha"
    Write-Host "     解包即用: 解压后执行 $name\bin\cjah.exe --check-syntax=true <file.cj>"
}

Main
