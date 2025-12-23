# CJASTHelper

Cangjie Abstract Syntax Tree Helper, 提供仓颉抽象语法树操作的扩展能力，包括打印源码（解糖后）等。

> 注意：
>
> 该工具是基于 Cangjie 开源代码开发，处于试验阶段。

## 构建

### 依赖下载

[Cangjie开源仓库](https://gitcode.com/Cangjie/cangjie_compiler)

参考 [Cangjie SDK构建指导](https://gitcode.com/Cangjie/cangjie_build) 按需构建相关组件。

[Json 开源仓库](https://github.com/nlohmann/json/releases/download/v3.12.0/include.zip)

### 环境变量配置

该工具构建依赖 Cangjie 开源仓源码中头文件 和 构建产物。 构建依赖 json 开源库 （头文件）。

假设本地下载的 Cangjie 源码目录为 `${xxx}/cangjie_compiler/`, 构建好的 SDK 目录为 `${yyy}/cangjie/` 。
假设本地下载的 json 源码目录为 `${third_party}/json` 。

> 注意 json目录下是 nlohmann/json.hpp

```bash
# 配置 Cangjie 源码路径
export CANGJIE_SRC_HOME=${xxx}/cangjie_compiler
export JSON_PATH=${third_party}/json
# 配置 Cangjie 二进制环境 (CANGJIE_HOME, LD_LIBRARY_PATH 环境变量)
source ${yyy}/cangjie/envsetup.sh
```

### 构建命令

相关依赖和环境设置成功后，使用 `build.sh` 构建本工具。

```bash
# 假设当前工具源码路径为 CJASTHelper_SRC
cd ${CJASTHelper_SRC}
# 构建 debug 版本工具
bash build.sh -t Debug -b
# 构建 release 版本工具
bash build.sh -t Release -b
```

## 功能列表

### 打印源码

提供 `--dump-source` 选项用来支持打印某个阶段后的源码，输出到指定目录，支持参数值：parse，deusgared-parse, sema，desugared-sema

- parse 用来打印语法解析后源码
- desugared-parse 用来打印语法解析并解糖后的源码
- sema   用来打印语义分析后的源码
- desugared-sema 用来打印语义分析并解糖后的源码

```bash
# 假设 构建好的工具 ${CJASTHelper_SRC}/build/bin/cjah 源码为 ${zzz}/main.cj 输出目录为 ${OUT_DIR}
${CJASTHelper_SRC}/build/bin/cjah --dump-source=desugared-sema ${zzz}/main.cj --output-dir ${OUT_DIR}
```

> 注意：
> 
> 该工具执行要依赖 Cangjie 构建的二进制环境，请确保 执行过 `source ${yyy}/cangjie/envsetup.sh`

### 配置是否打印解糖后代码

提供 `--dump-desugared` 选项用来配置是否打印解糖后的代码， 支持参数值：（默认值）true， false。

- true: 打印解糖后的代码
- false: 不打印解糖后的代码，尝试还原解糖前的用户代码。

```bash
# 假设 构建好的工具 ${CJASTHelper_SRC}/build/bin/cjah 源码为 ${zzz}/main.cj 输出目录为 ${OUT_DIR}
${CJASTHelper_SRC}/build/bin/cjah --dump-source=desugared-sema --dump-desugar=true --filter-decls=class,func ${zzz}/main.cj --output-dir ${OUT_DIR}
```

> 注意：
> 
> 当前选项功能尚未实现完成！

### 配置关注声明列表

提供 `--filter-decls` 选项用来配置打印的顶层声明种类列表，支持配置多个，支持参数值：func，class, interface，struct, enum, var

- 默认不过滤， 打印所有声明
- 配置多个值，例如 `--filter-decls=class,func` 仅打印顶层类、顶层函数。

```bash
# 假设 构建好的工具 ${CJASTHelper_SRC}/build/bin/cjah 源码为 ${zzz}/main.cj 输出目录为 ${OUT_DIR}
${CJASTHelper_SRC}/build/bin/cjah --dump-source=desugared-sema --filter-decls=class,func ${zzz}/main.cj --output-dir ${OUT_DIR}
```

## 测试用例

### 依赖下载

[gtest依赖下载](https://github.com/google/googletest/archive/tags/v1.17.0.zip)

> 注意：
>
> 下载后解压源码放到当前项目 `third_party/googletest-v1.17.0` 。
> 

### 构建

```bash
# -g 配置打开依赖 google-test 的测试构建
bash build.sh -g -t Release -b
```

> 注意：
>
> 构建成功会在生成测试可执行文件 `build/bin/cjah_test`
>

### 执行

```bash
# 执行所有用例
./build/bin/cjah_test

# 执行 CI 用例
./build/bin/cjah_test --gtest_filter="*/CJAHTest.CI001/*"
```
