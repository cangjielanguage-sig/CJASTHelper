# 构建指导（Linux / macOS）

本文档介绍 CJASTHelper 在 Linux（以及 macOS）平台上的依赖准备、环境变量配置与构建步骤。
Windows 平台请见 [build-windows.md](./build-windows.md)。

## 1. 依赖

| 依赖 | 用途 | 获取方式 |
|---|---|---|
| Cangjie 开源仓源码 | 提供 Cangjie 前端头文件（`include/cangjie/AST` 等） | [cangjie_compiler](https://gitcode.com/Cangjie/cangjie_compiler) |
| Cangjie SDK 构建产物 | 提供 `tools/lib/libcangjie-lsp.so`（链接）与运行时依赖 | 参考 [Cangjie SDK 构建指导](https://gitcode.com/Cangjie/cangjie_build) 按需构建 |
| nlohmann/json（头文件） | 配置解析 | [json v3.12.0 include.zip](https://github.com/nlohmann/json/releases/download/v3.12.0/include.zip)，解压后目录内应有 `nlohmann/json.hpp` |
| cmake / ninja / clang | 构建工具链 | 发行版包管理器 |

> 提示：本工具通过 `libcangjie-lsp`（动态库）复用 Cangjie 前端，因此构建时既需要 SDK 的
> **头文件**（编译），也需要 SDK 的 **动态库与运行时**（链接、运行）。

## 2. 环境变量

假设：

- Cangjie 源码目录为 `${xxx}/cangjie_compiler/`
- 构建好的 SDK 目录为 `${yyy}/cangjie/`
- json 目录为 `${third_party}/json`（目录下是 `nlohmann/json.hpp`）

```bash
# Cangjie 源码路径（构建脚本据此拼接 include 目录）
export CANGJIE_SRC_HOME=${xxx}/cangjie_compiler
# json 头文件路径（缺省使用仓库内 third_party/json/json）
export JSON_PATH=${third_party}/json
# 配置 Cangjie 二进制环境（CANGJIE_HOME、LD_LIBRARY_PATH 等）
source ${yyy}/cangjie/envsetup.sh
```

其中 `CANGJIE_HOME` 会被构建脚本用于拼接 `tools/lib` 动态库搜索路径（`-d` 选项可单独覆盖）。

## 3. 构建命令

```bash
# 进入本工具源码目录
cd ${CJASTHelper_SRC}

# 构建 debug 版本
bash build.sh -t Debug -b

# 构建 release 版本
bash build.sh -t Release -b
```

构建产物：

- 可执行文件：`build/bin/cjah`
- 动态库：`build/lib/`（`libutils.so`、`libcjast_wrapper.so`、`libcjast_helper_core.so`）
- Pass 插件：`build/bin/`（`libcjast_desugar_pass.so`、`libcjast_to_source_pass.so`）

### build.sh 选项

| 选项 | 说明 |
|---|---|
| `-h` | 显示帮助 |
| `-v` | 打印详细构建命令 |
| `-g` | 开启 googletest 测试构建（见 testing.md） |
| `-t` | 构建类型 `Debug` / `Release` |
| `-p` | 安装前缀（默认 `output`） |
| `-d` | 覆盖 Cangjie 库路径（默认 `$CANGJIE_HOME/tools/lib`） |
| `-u` | 仅更新 CMake 缓存 |
| `-b` | 构建 |
| `-i` | 安装（`cmake --install`，前缀由 `-p` 指定，默认 `output/`） |
| `-a` | **alone 安装**：把依赖的 SDK 动态库一并拷进安装目录，脱离 SDK / `LD_LIBRARY_PATH` 独立运行（需与 `-i` 同用） |
| `-c` | 清理 build/ 与 output/ |
| `-r` | 运行 cjah（配合 `-g` 则运行测试），参数放在 `--` 之后 |

### 安装（可选）

```bash
# 常规安装：产物依赖运行环境能找到 libcangjie-lsp.so（envsetup.sh 的 LD_LIBRARY_PATH）
bash build.sh -t Release -i

# alone 安装：把 libcangjie-lsp.so 一起拷进安装目录，安装后无需任何 Cangjie 环境变量（CANGJIE_HOME 除外）
bash build.sh -t Release -a -i
```

安装产物位于 `output/`（`-p <dir>` 可改），包含 `bin/`、`lib/` 与 `config/`。

alone 模式下安装目录的依赖是闭环的：

- 安装出的 `bin/cjah` 与 `lib/*.so` 使用相对 RPATH（Linux `$ORIGIN`，macOS `@loader_path`），
  因此互相引用不依赖绝对路径；
- `libcangjie-lsp.so`（macOS 为 `.dylib`）随包复制到 `lib/`，脚本用 `ldd`（macOS 用
  `install_name_tool` 改写为 `@rpath` 并重签名）校验无未解析依赖后才会报成功。

> 注意：alone 只闭环**动态库**。真正跑分析时 Cangjie 前端仍需 `CANGJIE_HOME` 定位标准库
> （与是否 alone 无关），纯 `--check-syntax` 等不触达标准库的能力则可完全无环境变量运行。

### 打包发布

`scripts/package_release.sh` 把安装目录按版本号打成可分发的 `tar.gz`（默认输出到 `dist/`）：

```bash
# 完整版 -> dist/cjah_alpha_0.0.7.tar.gz
bash scripts/package_release.sh -v alpha_0.0.7

# 最小版（仅语法检查）-> dist/cjah_minimal_alpha_0.0.7.tar.gz
bash scripts/package_release.sh -v alpha_0.0.7 --minimal
```

| 选项 | 说明 |
|---|---|
| `-v, --version <ver>` | 版本号（必填），如 `alpha_0.0.7`，仅允许字母数字与 `.` `_` `-` |
| `-m, --minimal` | 生成最小版：裁掉 pass 插件、`valid_options.json` 只留 `check-syntax` |
| `-p, --prefix <dir>` | 安装目录（默认 `output/`，应先用 `-a -i` 安装） |
| `-o, --out <dir>` | 输出目录（默认 `dist/`） |
| `-k, --keep` | 保留暂存目录 |

最小版内容：`bin/cjah`、`lib/{libcangjie-lsp,libcjast_helper_core,libcjast_wrapper,libutils}.so`、
`config/valid_options.json`（**不含** `passes.json`、任务配置与 pass 插件）。check-syntax 只跑 parse
阶段，既不 `dlopen` pass 插件（`LD_DEBUG=files` 验证），也不加载 pass 配置
（`ArgHelper::ConfigGlobalOptions` 在 check-syntax 下跳过 `PassManager::Init`）。

脚本在打 tar 包前会对**暂存目录**做真实自检，任一失败就中止并保留暂存目录：

1. 单文件语法正确 → 退出码 0；
2. 单文件语法错误 → 退出码非 0；
3. 目录输入（多包分组）→ 退出码 0；
4. 未知工具选项 → 退出码非 0（不再「只打帮助就返回 0」）；
5. `--help` 只列出允许的选项（最小版仅 `--check-syntax`），被裁掉的选项（如
   `--dump-source`）实调必须非 0；
6. 最小版无 pass 插件残留、`config/` 仅含 `valid_options.json`；
7. 动态库依赖闭环（Linux `ldd` / macOS `otool`，覆盖 `bin/cjah` 与 `lib/*` 全部二进制，
   无未解析或指向包外的引用）。

解包即用：

```bash
tar -xzf dist/cjah_minimal_alpha_0.0.7.tar.gz
./cjah_minimal_alpha_0.0.7/bin/cjah --check-syntax=true some.cj   # 无需任何环境变量
```

> Windows 上参数与自检用例完全一致的脚本是 `scripts/package_release.ps1`
> （产物为 `.zip`，依赖闭环改用 PE 导入表检查），详见
> [build-windows.md §6](./build-windows.md#6-打包发布scriptspackage_releaseps1)。

## 4. 验证

```bash
# 打印工具选项（无需 SDK 运行环境）
./build/bin/cjah --help
```

完整运行需要 Cangjie 二进制环境（已执行过 `source ${yyy}/cangjie/envsetup.sh`），例如：

```bash
./build/bin/cjah --dump-source=parse test/data/inputs/main.cj --output-dir /tmp/out
```

更详细的运行方式见 [usage.md](./usage.md)。