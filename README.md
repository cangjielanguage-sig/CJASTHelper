# CJASTHelper

Cangjie Abstract Syntax Tree Helper —— 基于 Cangjie 开源编译器前端（`libcangjie-lsp`）
的 AST 操作与源码转换工具，提供多阶段源码打印（含解糖后）、AST 转储、纯语法检查、
宏展开控制与 Pass 插件化分析管线。

> 注意：该工具基于 Cangjie 开源代码开发，处于试验阶段。

## 特性一览

| 能力 | 说明 | 用法 |
|---|---|---|
| 多阶段源码打印 | `parse` / `desugared-parse` / `macro` / `sema` / `desugared-sema` | `--dump-source` |
| AST 转储 | 将 AST 以文本形式输出到文件 | `--dump-ast` |
| 纯语法检查 | 文件或目录输入（目录自动按包分组检查），支持合并 JSON 诊断 | `--check-syntax` |
| 宏展开控制 | 按需开启/关闭宏展开 | `--enable-macro` |
| 声明过滤 | 只输出指定种类的顶层声明 | `--filter-decls` |
| Pass 插件管线 | 解糖、源码还原（含 Java 输出）以插件动态库形式加载 | `--pass-config` + `passes.json` |
| CJC 选项透传 | 未识别的参数原样透传给前端（`--output-dir`、`-p` 等） | 参见 `cjc -h` |

## 快速开始

### 依赖

- Cangjie SDK（编译头文件 + 构建产物），参考
  [Cangjie SDK 构建指导](https://gitcode.com/Cangjie/cangjie_build) 从
  [Cangjie 开源仓库](https://gitcode.com/Cangjie/cangjie_compiler) 构建；
- nlohmann/json 头文件（[v3.12.0](https://github.com/nlohmann/json/releases/download/v3.12.0/include.zip)）；
- CMake、Ninja、clang（Linux/macOS），CMake、Ninja、llvm-mingw
  （Windows：mstorsjo/llvm-mingw [20220906](https://github.com/mstorsjo/llvm-mingw/archive/refs/tags/20220906.tar.gz)
  源码构建，`--with-default-msvcrt=msvcrt` 后端）。

### 构建

Linux / macOS：

```bash
export CANGJIE_SRC_HOME=${xxx}/cangjie_compiler   # Cangjie 源码（提供 include）
export JSON_PATH=${third_party}/json              # nlohmann/json 目录（可选，默认 third_party/json）
source ${yyy}/cangjie/envsetup.sh                 # 配置 CANGJIE_HOME、LD_LIBRARY_PATH
bash build.sh -t Release -b
```

Windows：

```powershell
# 机器路径（SDK、llvm-mingw、ninja 等）集中配置在 scripts/win_env.ps1，可用环境变量覆盖
# 构建选项与 build.sh 对齐（短选项两侧通用）
build.bat -b -t Release
# 或：powershell -File build.ps1 -b -t Release
```

构建产物位于 `build/bin/`（`cjah` / `cjah.exe`）；安装到 `output/` 用 `-i` / `bash build.sh -t Release -i`。
详细步骤见 [构建指导](./doc/build.md) 与 [Windows 构建指导](./doc/build-windows.md)。

### 运行

```bash
mkdir -p out
# 打印语义分析并解糖后的源码（sema 及之后阶段需透传 --output-type）
./build/bin/cjah --dump-source=desugared-sema --output-type=dylib main.cj --output-dir out
# 产物：out/main_source.cj

# 纯语法检查（目录输入，合并 JSON 诊断）
./build/bin/cjah --check-syntax=true src/ --diagnostic-format=json
```

未识别的参数会透传给前端（`-p <包路径>`、`--output-dir`、`-Woff` 等），完整选项见
[功能与命令行参考](./doc/usage.md)。

## 文档导航

| 文档 | 内容 |
|---|---|
| [doc/build.md](./doc/build.md) | Linux / macOS 构建指导 |
| [doc/build-windows.md](./doc/build-windows.md) | Windows 构建指导（工具链、路径配置、常见问题） |
| [doc/usage.md](./doc/usage.md) | 功能与命令行参考（选项、阶段、输出约定、示例） |
| [doc/design.md](./doc/design.md) | 架构设计（模块划分、动态库依赖、阶段管线） |
| [doc/testing.md](./doc/testing.md) | 测试套件构建与运行 |

## 测试

基于 googletest v1.17.0（`third_party/googletest-v1.17.0` 或 `GTEST_RELEASE_PATH` 预编译）：

```bash
bash build.sh -g -t Release -b                 # Linux/macOS
powershell -File build.ps1 -g -b   # Windows
./build/bin/cjah_test --gtest_filter="*/CJAHTest.CI001/*"   # CI 管线回归用例
```

详见 [测试指导](./doc/testing.md)。

## License

[Apache License 2.0](./LICENSE)