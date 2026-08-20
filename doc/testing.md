# 测试指导

本文档介绍 CJASTHelper 测试套件（googletest）的构建与运行。

## 1. 依赖：googletest v1.17.0

二选一：

1. **预编译安装**（推荐）：将 gtest 安装到本地目录（含 `lib/`、`include/`），
   通过环境变量 `GTEST_RELEASE_PATH` 指向它。Windows 上 `scripts/win_env.ps1`
   默认值为 `D:/sdks/googletest`，可用环境变量覆盖。
2. **源码构建**：下载
   [googletest v1.17.0](https://github.com/google/googletest/archive/tags/v1.17.0.zip)
   解压到 `third_party/googletest-v1.17.0`（由 CMake 自动 add_subdirectory 构建）；
   此时不要设置 `GTEST_RELEASE_PATH`。

> 未满足上述任一条件时，CMake 会在开启测试时直接报错并提示下载地址。

## 2. 构建测试

### Linux / macOS

```bash
# -g 开启测试构建
bash build.sh -g -t Release -b
# 产物：build/bin/cjah_test
```

### Windows

```powershell
# GTEST_RELEASE_PATH 在 scripts/win_env.ps1 中配置（默认 D:/sdks/googletest）
powershell -File build.ps1 -g -t Debug -b
# 产物：build/bin/cjah_test.exe
```

## 3. 运行

```bash
# 运行全部用例
./build/bin/cjah_test

# 只运行 CI 用例（管线回归：各阶段 × 解糖开关、迭代不动点）
./build/bin/cjah_test --gtest_filter="*/CJAHTest.CI001/*"

# 通过构建脚本运行（配合 -g 时 -r 运行测试；未构建会自动先构建）
powershell -File build.ps1 -g -r
```

CTest 方式：

```bash
ctest --test-dir build -R CJAHTest
```

## 4. 用例覆盖

| 套件实例 | 覆盖内容 |
|---|---|
| `IterateAllStages` | 对 `test/data/inputs/desugar.cj` 遍历 `parse / desugared-parse / sema / desugared-sema` 各阶段与解糖开关的组合，输出与 `test/data/expected/` 下的 golden 文件逐字节比对 |
| `IterateFP` | 不动点迭代：对 `main.cj` 连续迭代 `--dump-source=desugared-sema` 三次，验证结果收敛 |
| `CheckSyntaxDirTest` | `--check-syntax` 目录模式的包分组、参数透传、单文件/同目录/多包等场景 |

测试数据布局：

```
test/data/
├── inputs/       # 输入 .cj（desugar.cj、main.cj、macro.cj、annotation.cj、bad_syntax.cj 等）
├── expected/     # golden 输出（<用例>/<stage>_<desugar>.cj）
└── output/       # 运行期输出目录（由测试自动清理）
```

## 5. 更新 golden 数据

`GenGolden` 用例会以实际输出**覆盖** expected 目录中的 golden 文件，用于确认新行为
正确后更新基线：

```bash
./build/bin/cjah_test --gtest_filter="*/CJAHTest.GenGolden/*"
```

> 覆盖前请人工核对输出正确性——golden 一旦更新即成为新的回归基线。