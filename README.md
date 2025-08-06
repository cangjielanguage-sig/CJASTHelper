# CJASTHelper

Cangjie Abstract Syntax Tree Helper, 提供仓颉抽象语法树操作的扩展能力，包括打印源码（解糖后）等。

## 构建

### 依赖下载

### 环境变量配置

### 构建命令

```bash
bash build.sh -t Debug -b
```

## 功能列表

### 打印解糖后源码

提供 `--dump-source` 选项用来支持打印某个阶段后的源码，输出到指定目录，支持参数值：parser，deusgared-parser, sema，desugared-sema

- parse 用来打印语法解析后源码
- desugared-parse 用来打印语法解析并解糖后的源码
- sema   用来打印语义分析后的源码
- desugared-sema 用来打印语义分析并解糖后的源码

```bash
cjah --dump-source=desugar-sema --output-dir=out
```
