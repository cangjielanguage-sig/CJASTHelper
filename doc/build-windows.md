# 构建指导（Windows）

本文档介绍 CJASTHelper 在 Windows 上的构建与运行。Windows 构建使用
CMake + Ninja + LLVM MinGW（clang++）工具链，入口脚本为 `build.ps1`（`build.bat` 是其薄包装）。

## 1. 前置条件

| 依赖 | 说明 |
|---|---|
| Cangjie Windows SDK | 提供编译头文件（`include/cangjie/...`）、链接导入库（`tools/lib/libcangjie-lsp.dll.a`）与运行时 `tools/bin/libcangjie-lsp.dll` |
| llvm-mingw 工具链 | `x86_64-w64-mingw32-clang/clang++`、`llvm-ar`、`llvm-ranlib`，并随包提供 `libssp-0.dll`；**必须使用 `20220906` 版本、`msvcrt` 后端**（见下） |
| CMake（3.16.5+） | 构建系统 |
| Ninja | 构建执行器 |
| nlohmann/json（头文件） | 仓库 `third_party/json/` 或自行下载放入 |

### 工具链来源（llvm-mingw）

Windows 构建使用 [mstorsjo/llvm-mingw](https://github.com/mstorsjo/llvm-mingw) 官方仓库
**`20220906` tag 源码构建**的工具链：

- 源码包：[llvm-mingw-20220906.tar.gz](https://github.com/mstorsjo/llvm-mingw/archive/refs/tags/20220906.tar.gz)
- 构建配置：configure 时指定 **`--with-default-msvcrt=msvcrt`**（默认 C 运行时为 msvcrt 后端，
  而非工具链默认的 ucrt）
- 对应本机目录形如 `llvm-mingw-20220906-msvcrt-x86_64/`，`MINGW_BIN` 指向其 `bin/`

> 请勿混用其它版本 / 其它 C 运行时后端（ucrt）构建的工具链：运行时依赖
> （`libssp-0.dll`、`libc++.dll` 等）与 ABI 行为均以 20220906 + msvcrt 后端为准。

> Windows 上 Cangjie SDK 分为「官方完整 SDK」与「DevEco/ohpm 渠道工具链」两类，本工具需要
> **包含 C++ include 头文件**的那一套（官方完整 SDK）。

## 2. 环境路径配置（scripts/win_env.ps1）

机器相关的路径全部集中在 `scripts/win_env.ps1`，由 `build.ps1` 自动加载，
优先级为：**同名环境变量 > 文件内默认值**。不修改脚本即可临时切换（CI、换机器、多 SDK 并存）：

| 变量 | 默认值（示例机器） | 说明 |
|---|---|---|
| `MINGW_BIN` | `D:/sdks/llvm-mingw-20220906-msvcrt-x86_64/bin` | llvm-mingw（20220906，msvcrt 后端）的 bin 目录 |
| `CANGJIE_HOME` | `D:/sdks/cangjie` | Cangjie SDK 根目录 |
| `SCOOP_SHIMS` | `C:/Users/<name>/scoop/shims` | ninja 所在 shims 目录（追加进 PATH） |
| `GTEST_RELEASE_PATH` | `D:/sdks/googletest` | googletest 预编译安装目录（仅 `-g` 需要） |

切换示例：

```powershell
$env:MINGW_BIN   = "D:/sdks/llvm-mingw-15/bin"
$env:CANGJIE_HOME = "D:/sdks/cangjie"
powershell -File build.ps1 -t Debug -b
```

> **重要：`CANGJIE_HOME` 必须与提供编译头文件的 SDK 为同一套。**
> 编译按头文件定义的类布局构造 `CompilerInvocation` 等对象，运行时跨 DLL 传给
> `libcangjie-lsp.dll`；若头文件与动态库来自不同 SDK 版本（例如头文件用完整 SDK、
> 环境变量指向 DevEco 6.1 渠道工具链），类成员偏移不一致，会在启动后直接
> **段错误（SIGSEGV，退出码 139）**。`build.ps1` 会在构建/运行前统一 `CANGJIE_HOME`。

## 3. 构建命令

```powershell
# 直接使用 build.bat（推荐，等价于调用 build.ps1）
build.bat -b -t Debug

# 或显式调用 PowerShell 脚本
powershell -ExecutionPolicy Bypass -File build.ps1 -v -t Debug -b

# release 构建
powershell -File build.ps1 -t Release -b

# 构建并安装到 output/
powershell -File build.ps1 -t Release -i
```

### build.ps1 选项

选项与 Linux 版 `build.sh` **完全一致**（短选项，同一命令可直接跨平台复用）：

| 选项 | 说明 |
|---|---|
| `-h` | 显示帮助 |
| `-v` | 打印实际执行的构建命令（等价传递 `-v` 给 Ninja） |
| `-g` | 开启 googletest 测试构建（见 testing.md） |
| `-t` | 构建类型 `Debug` / `Release`（默认 **Debug**，与 build.sh 一致） |
| `-p` | 安装前缀（默认 `output`） |
| `-d` | 覆盖 Cangjie 库路径（Windows 下会同时统一 `CANGJIE_HOME`，见下文警告） |
| `-u` | 仅更新 CMake 缓存 |
| `-b` | 仅构建 |
| `-i` | 构建并安装（`cmake --install`，前缀由 `-p` 指定，默认 `output`） |
| `-a` | **alone 安装**：把 `cangjie-lsp.dll` 与工具链运行时 DLL 一并拷到 `bin/`，脱离 `PATH` / `CANGJIE_HOME` 独立运行（需与 `-i` 同用） |
| `-c` | 清理 build/ 与 output/ |
| `-r` | 构建后运行 cjah（配合 `-g` 则运行测试），参数放在 `--` 之后 |

唯一平台差异是 `-d` 在 Windows 上额外统一 `CANGJIE_HOME`（防跨 SDK ABI 崩溃，
构建脚本会强制完成）。

### 构建自动完成的事

成功构建后脚本会自动：

1. 将 SDK 运行时 DLL 复制到 `build/bin/`：
   `libcangjie-lsp.dll`、`libc++.dll`、`libunwind.dll`、`libwinpthread-1.dll`
   （exe 所在目录在 DLL 搜索顺序中优先级最高，避免依赖 PATH）
2. 将 `config/*.json` 部署到 `build/bin/config/`（工具配置查找的候选位置）
3. `-i` 时对 `output/bin/` 执行同样的部署
4. `-a -i`（alone 模式）时，额外把 `cangjie-lsp.dll`（探测 `tools/lib`、`tools/bin`）与
   `libc++.dll`、`libunwind.dll`、`libwinpthread-1.dll`、`libssp-0.dll` 拷到 `output/bin/`，
   使安装产物不依赖 `PATH` / `CANGJIE_HOME` 即可启动

## 4. 产物

```
build/
├── bin/
│   ├── cjah.exe                  # 主程序
│   ├── cjah_test.exe             # 测试程序（-g 时）
│   ├── cjast_helper_core.dll     # 核心模块
│   ├── cjast_wrapper.dll         # 前端封装模块
│   ├── utils.dll                 # 工具模块
│   ├── cjast_desugar_pass.dll    # 解糖 pass 插件
│   ├── cjast_to_source_pass.dll  # 源码还原 pass 插件
│   ├── config/                   # valid_options.json / passes.json
│   └── （SDK 运行时 DLL：libcangjie-lsp.dll 等）
└── lib/                          # 导入库（*.dll.a）
```

`output/` 为安装前缀（`output/bin/`、`output/lib/`、`output/config/`），结构与 `build/` 对应。

## 5. 运行

```powershell
# 快速验证：语法检查
./build/bin/cjah.exe --check-syntax=true test\data\inputs\main.cj

# 打印 parse 阶段源码到指定目录
./build/bin/cjah.exe --dump-source=parse test\data\inputs\main.cj --output-dir build\bin\tmp_out

# 通过 build.ps1 运行（未构建会自动先构建）
powershell -File build.ps1 -r -- --dump-source=sema test\data\inputs\main.cj --output-dir build\bin\tmp_out
```

注意：

- **路径请使用 Windows 格式**（`test\data\...` 或 `D:/...`）。在 git-bash/MSYS 中
  `/d/...` 形式的路径会触发前端报错 `error: invalid file or directory`。
- 诊断信息（语法错误等）输出到 **stderr**。
- 若本机 PATH 中同时存在多个 Cangjie SDK（如 DevEco 6.1 渠道），请确保
  `CANGJIE_HOME` 与编译所用 SDK 一致后再运行（参见上文警告）。

## 6. 打包发布（scripts/package_release.ps1）

构建脚本只负责产出安装目录（`output/`），分发用的压缩包由打包脚本生成
（对应 Linux/macOS 的 `scripts/package_release.sh`）：

```powershell
# 完整版（bin\ + config\ + lib\，含全部 pass 插件）
powershell -ExecutionPolicy Bypass -File scripts\package_release.ps1 -v alpha_0.0.7

# 最小版（仅 --check-syntax：裁掉 pass 插件，config 只留 valid_options.json）
powershell -File scripts\package_release.ps1 -v alpha_0.0.7 -m
```

产物为 `dist\cjah_<version>.zip` / `dist\cjah_minimal_<version>.zip`（解压即用，
顶层目录即包名），脚本结尾打印大小与 sha256。

参数与 sh 版一一对应：

| 选项 | 说明 |
|---|---|
| `-v, --version` | 版本号（必填） |
| `-m, --minimal` | 生成最小版 |
| `-p, --prefix` | 安装目录（默认 `output`，需先 `build.ps1 -a -i`） |
| `-o, --out` | 输出目录（默认 `dist`） |
| `-k, --keep` | 保留暂存目录 |

打包前必须用 **alone 模式**安装（`-a -i`）：脚本会检查 `bin\libcangjie-lsp.dll`
是否存在，缺失直接报错（否则产物无法脱离 SDK 运行）。

自检用例全部在**清空环境变量**的子进程里执行（只保留 `SystemRoot`/`SystemDrive`/
`TEMP`/`TMP`），因此通过即证明产物不依赖 `CANGJIE_HOME` / `PATH` / `MINGW_BIN`：

| 用例 | 期望 |
|---|---|
| 单文件语法检查 | 退出码 0 |
| 语法错误单文件 | 退出码非 0 |
| 多包目录 / 目录内含错 | 0 / 非 0 |
| `--output-dir <dir>` 两段式写法 | 语法错误仍被判错 |
| 未知工具选项 | 退出码非 0（不再「只打帮助就返回 0」） |
| `--help` | 列出 `--check-syntax`；最小版只列这一项，被裁掉的选项实调必须非 0 |
| 完整版 `--dump-source=parse` | 退出码 0 且有 `*_source.cj` 产物（pass 插件可加载） |
| PE 导入表（静态） | 所有非系统导入均命中包内 `bin\`（`ldd`/`otool` 的 Windows 对应物） |
| 解包验证 | 解压到含空格/中文的路径后重跑关键用例 |

平台差异（与 sh 版）：

- 归档格式为 **zip**（`System32\tar.exe` 生成，条目分隔符为 `/`；无 tar 时回退
  `Compress-Archive`）；
- 依赖闭环检查用 PE 导入表（`llvm-objdump` / `objdump`），而非 `ldd` / `otool -L`；
- 「只读当前目录」用例跳过（Windows 无 chmod 语义），由 Linux/macOS 版覆盖。

> **独立使用的边界（实测）**：`--check-syntax`、`--dump-source=parse` 等 parse 阶段
> 能力在清空环境下可用；`--dump-source=sema`、macro 展开等需要调用 Cangjie 编译器
> 驱动的阶段仍要求 `CANGJIE_HOME` 指向有效 SDK（报错
> `failed to detect cangjie home`）——这是前端能力边界，不是打包缺陷。

## 7. 常见问题

### 7.1 构建报 `Please set correct CANGJIE_INCLUDE`（CANGJIE_HOME 被 DevEco 渠道 SDK 抢占）

DevEco Studio / ohpm 安装时会在**用户环境变量**里写入 `CANGJIE_HOME`，指向
`.cangjie-sdk/<版本>/cangjie/build-tools`——该渠道工具链**不含 C++ include 头**，
build.ps1 会优先使用该环境变量，于是 CMake 报错退出。

解决（任选其一，推荐 1）：

1. 构建时用 `-d` 显式指定完整 SDK（同时统一 `CANGJIE_HOME`）：

```powershell
build.bat -b -t Debug -d D:/sdks/cangjie
```

2. 临时覆盖环境变量：

```powershell
$env:CANGJIE_HOME = "D:/sdks/cangjie"
build.bat -b -t Debug
```

3. 修改 `scripts/win_env.ps1` 的默认 `CANGJIE_HOME`，或删除 DevEco 写入的用户级
   `CANGJIE_HOME`（会影响 DevEco 自身，不推荐）。

### 7.2 启动即段错误（SIGSEGV / 退出码 139）

症状：cjah 打印若干行 `find ... json` 配置查找日志后直接崩溃，没有任何 `Exception:` 输出。

原因：编译头文件、链接导入库、运行时 DLL 三者来自**不同版本**的 SDK，跨 DLL C++ ABI
不兼容（类成员偏移不一致）。

排查与修复：

1. 确认 `CANGJIE_HOME` 与 `win_env.ps1` / `-d` 指向同一 SDK；
2. 用同参数直接跑该 SDK 自带的 `cjc` 不崩，即证明参数没问题；
3. 修复后重建：

```powershell
powershell -File build.ps1 -t Debug -b
```

### 7.3 缺 DLL：`error while loading shared libraries: utils.dll`

提示可能具有误导性。实际缺失的往往是 `libssp-0.dll`（llvm-mingw 的 stack protector
运行库，重编译后新增的依赖）。检查依赖清单：

```bash
objdump -p build/bin/cjah.exe | grep "DLL Name"
```

修复：从 `D:/sdks/llvm-mingw-*/x86_64-w64-mingw32/bin/libssp-0.dll`（或 `output/bin/`）
复制到 `build/bin/`。重编译后建议对照 `output/bin/` 与 `build/bin/` 的 DLL 集合。
新版本已通过 CMake 安装规则与构建脚本自动部署。

### 7.4 check-syntax 的退出码

- 有语法错误：退出码 1；
- 无效目录：打印 `error: invalid file or directory '...'`，但退出码为 **0**；
- 空目录：警告 `warning: no .cj file found in directory: ...`，退出码 0。

退出码不能作为唯一判据，请以 stderr 诊断内容为准。

### 7.5 配置查找日志（find ... json）不是错误

工具按 exe 目录依次查找 `./config/`、`../config/` 等位置加载 `valid_options.json` /
`passes.json`，找不到会打印日志；构建脚本已将 `config/*.json` 部署到 `build/bin/config/`，
日志属于正常行为，非致命。