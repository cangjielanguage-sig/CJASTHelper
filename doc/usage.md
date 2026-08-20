# 功能与命令行参考

本文档介绍 cjah 的命令行用法、各阶段语义与输出约定。构建方法见 [build.md](./build.md)
（Linux）与 [build-windows.md](./build-windows.md)；架构见 [design.md](./design.md)。

## 1. 命令行结构

```
cjah [options] [cjc-options]
```

- `options`：本工具选项（下表）。未识别的参数**原样透传**给 Cangjie 前端
  （`libcangjie-lsp`），即上方示例中的 `cjc-options`，可用 `cjc -h` 查询其含义。
- 不带任何工具选项时打印帮助信息。

内置帮助：

```bash
./build/bin/cjah --help
```

## 2. 选项一览

| 选项 | 值 | 默认 | 说明 |
|---|---|---|---|
| `--dump-source` | `parse`、`desugared-parse`、`macro`、`sema`、`desugared-sema` | 无 | 打印指定阶段后的源码 |
| `--dump-ast` | 文件路径 | 无 | 将 AST 转储到 `<路径>/<文件名>.ast` |
| `--check-syntax` | `true` / `false` | `false` | 仅做语法检查，跳过分析/转换；输入可为文件或目录 |
| `--enable-macro` | `true` / `false` | `true` | 是否执行宏展开 |
| `--filter-decls` | `func`、`var`、`struct`、`enum`、`interface`、`class` | 不过滤 | 只输出指定种类的顶层声明，可多个，逗号分隔 |
| `--pass-config` | 文件名 | `passes.json` | Pass 配置文件 |

输出目录由透传的 cjc 选项 `--output-dir <dir>` 指定（不传时输出到当前目录）。

## 3. 阶段管线与 --dump-source

前端按阶段顺序执行：

```
parse → (desugared-parse) → macro → sema → desugared-sema
```

- `parse`：语法解析 + 条件编译（`PerformConditionCompile`）
- `desugared-parse`：解析后立即解糖的源码
- `macro`：宏展开后的源码（受 `--enable-macro` 控制）
- `sema`：语义分析后的源码
- `desugared-sema`：语义分析并解糖后的源码

`--dump-source=<stage>` 打印该阶段完成后的源码。实际执行 `>= stage` 之前的所有阶段，
其中 `desugared-parse` 在目标阶段不低于 `sema` 时会被跳过。

> **注意：`sema` 与 `desugared-sema` 阶段需要透传前端选项 `--output-type`
> （例如 `--output-type=dylib`）指定输出类型，否则前端在语义分析阶段报错。**

### 输出约定

- 输出文件与输入同名、加 `_source` 后缀：`main.cj → {output-dir}/main_source.cj`
- 输出目录须已存在（或由调用方创建）

```bash
# 示例：打印语义分析并解糖后的源码（sema 及之后阶段需透传 --output-type）
mkdir -p out
./build/bin/cjah --dump-source=desugared-sema --output-type=dylib main.cj --output-dir out
# 产物：out/main_source.cj
```

```bash
# 只保留顶层函数与类
./build/bin/cjah --dump-source=desugared-sema --filter-decls=func,class --output-type=dylib main.cj --output-dir out
```

## 4. --dump-ast：AST 转储

将 parse 之后（分析之前）的 AST 以文本形式转储到文件（`<路径>` 为**已存在的目录**，
文件名取源文件全名）：

```bash
mkdir -p ast_out
./build/bin/cjah --dump-source=parse --dump-ast=ast_out main.cj
# 产物：ast_out/main.cj.ast
```

## 5. --check-syntax：纯语法检查

`--check-syntax=true` 时只做语法解析并上报语法错误，**跳过** AST 转储、语义分析与
Pass 转换，无需指定 `--dump-source`。

- **文件输入**：`cjah --check-syntax=true a.cj`
- **目录输入**：递归收集 `.cj` 文件，按包分组——每个**直接包含 `.cj` 文件**的目录
  视为一个包，每个包独立一次前端调用检查（避免多包文件混在一次调用中漏报错误）。

### 诊断输出

- 诊断（含 `--diagnostic-format=json`）输出到 **stderr**，stdout 恒为空；
- JSON 模式（透传 `--diagnostic-format=json`）下，目录多包检查会把所有包的诊断
  去重、按（文件, 行, 列）排序后合并为**一份** `{"Diags": [...]}` 文档输出；
- 每条诊断字段为 PascalCase：`DiagKind / Severity / DiagCategory / Message /
  Location{File,Line,Column} / MainHint{Content,Range{Begin,End}} / OtherHints /
  Notes / Helps`。

### 退出码

| 场景 | 退出码 |
|---|---|
| 全部通过 / 语法错误为 0 | 0 |
| 存在语法错误 | 1 |
| 无效目录（`error: invalid file or directory`） | 0 |
| 空目录（`warning: no .cj file found in directory`） | 0 |

> 退出码不能作为唯一判据，请结合 stderr 诊断判断。

```bash
# 文件模式，文本诊断
./build/bin/cjah --check-syntax=true main.cj

# 目录模式，合并 JSON 诊断
./build/bin/cjah --check-syntax=true src/ --diagnostic-format=json
```

## 6. --enable-macro：宏展开

- `true`（默认）：执行宏展开。需要 Cangjie 宏运行时与宏库位于 `CANGJIE_HOME` 内；
- `false`：跳过宏展开（无宏使用时）。

```bash
./build/bin/cjah --dump-source=macro --enable-macro=true macro.cj --output-dir out
```

## 7. Pass 管线与 --pass-config

`--dump-source` 的源码输出由**源码还原 Pass**（to-source）在指定阶段完成后执行；
解糖、还原等逻辑以 Pass 插件形式组织，由配置加载：

- 配置文件默认 `passes.json`（可用 `--pass-config` 指定），位于可执行文件旁的
  `config/`（部署位置参见构建文档）；
- 每个 Pass 组对应一个插件动态库，从可执行文件所在目录加载：
  - `desugar` 组 → `cjast_desugar_pass`（解糖检查/还原/替换）
  - `to-source` 组 → `cjast_to_source_pass`（`to-cangjie` 源码还原，另有
    `to-java` 转换能力）
- `to-source` 组声明依赖 `desugar` 组，加载时按依赖顺序执行。

示例 `passes.json`：

```json
[
  { "group": "desugar",  "names": ["check-desugar", "recover-desugar", "replace-desugar"],
    "description": "Desugar pass", "version": "0.0.1", "lib": "cjast_desugar_pass", "dependencies": [] },
  { "group": "to-source", "names": ["to-cangjie", "to-java"],
    "description": "ToSource pass", "version": "0.0.1", "lib": "cjast_to_source_pass",
    "dependencies": ["desugar"] }
]
```

## 8. 透传的 cjc 选项

以下参数不在工具自有选项中，会交给前端处理（常用举例）：

| 参数 | 用途 |
|---|---|
| `-p <包路径>` | 以目录形式编译/检查一个包（`--package`），此时不传单个源文件 |
| `--output-dir <dir>` | 源码/AST 输出目录 |
| `--diagnostic-format=json` | JSON 诊断格式 |
| `-Woff <类别>` | 关闭某类警告 |

完整列表见 `cjc -h`。示例：

```bash
# 以包目录为输入（-p 要求目录包结构：目录名与包名对应）
./build/bin/cjah --dump-source=parse -p <pkg-dir> --output-dir out

# 静默处理：关闭 unused 与 parser 警告
./build/bin/cjah --dump-source=parse -Woff unused -Woff parser main.cj --output-dir out
```

> 提示：`--check-syntax` 目录模式已自动按包分组检查，**不要**再叠加透传 `-p`
> （前端会报 `not support compiling packages with source file`）。

## 9. 常用示例汇总

```bash
# 1) 打印 parse 阶段源码
cjah --dump-source=parse main.cj --output-dir out

# 2) 打印语义分析并解糖后的源码，只看顶层函数和类（需透传 --output-type）
cjah --dump-source=desugared-sema --filter-decls=func,class --output-type=dylib main.cj --output-dir out

# 3) 宏展开阶段源码
cjah --dump-source=macro macro.cj --output-dir out

# 4) 纯语法检查（文件）
cjah --check-syntax=true main.cj

# 5) 纯语法检查（目录，合并 JSON 诊断）
cjah --check-syntax=true src/ --diagnostic-format=json

# 6) AST 转储
cjah --dump-source=parse --dump-ast=ast_out main.cj
```