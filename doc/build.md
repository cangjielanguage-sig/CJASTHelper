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
| `-i` | 安装（`ninja install`，前缀为 `output/`） |
| `-c` | 清理 build/ 与 output/ |
| `-r` | 运行 cjah（配合 `-g` 则运行测试），参数放在 `--` 之后 |

### 安装（可选）

```bash
bash build.sh -t Release -i
```

安装产物位于 `output/`，包含 `bin/`、`lib/` 与 `config/`。

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