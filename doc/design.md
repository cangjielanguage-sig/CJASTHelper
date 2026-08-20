# 架构设计

## 组件图

![架构图](./svgs/framework.svg)

## 模块划分

| 模块 | 功能 | 依赖 |
|---|---|---|
| `utils` | 工具类封装（Logger、ArgParser、Printer、FileHelper、LibraryLoader、TaskExecutor 等） | 仅标准库 |
| `wrapper` | 封装外部模块 Cangjie 前端（`libcangjie-lsp` 动态库），提供 `CompilerInvocation`/`CompilerInstance` 复用与 `AstNode` 接口封装 | utils |
| `core` | 核心模块：AST 树遍历与分析 Pass 的核心接口定义，`AstHelper` 组织主流程 | utils、wrapper |
| ├─ `visitor` | AST 节点访问器（ConstAstVisitor / MutAstVisitor / VisitorBase） | utils、wrapper |
| └─ `pass` | Pass 接口定义与 PassManager | visitor、wrapper、utils |
| `passes` | 独立编译的 Pass 插件动态库（desugar / to-source），由 core 通过 LibraryLoader 动态加载 | core 接口 |
| `main` | 主入口（Main.cpp），解析参数、调度阶段与 Pass | wrapper、core、utils |

## 动态库划分

按功能职责划分为以下独立动态库（Linux/`lib*.so`，Windows/`*.dll`）：

| 构件 | 包含内容 |
|---|---|
| `utils` | Logger、ArgHelper、Printer 等工具类 |
| `cjast_wrapper` | AstNodeHelper、CangjieFrontendHelper 等前端封装类 |
| `cjast_helper_core` | AstHelper 组织主类、ConstAstVisitor/MutAstVisitor 等访问者、Pass 接口与 PassManager |
| `cjast_desugar_pass` | DesugarPass（check-desugar / recover-desugar / replace-desugar） |
| `cjast_to_source_pass` | ToSourcePass（to-cangjie / to-java），依赖 ConstAstVisitor 将 AST 还原为源码 |
| `cjah` | 主程序可执行文件 |

### 动态库间依赖关系

```
cjah (可执行文件)
    ↓
cjast_helper_core → cjast_wrapper → libcangjie-lsp (Cangjie 前端)
    ↓                   ↓
passes (插件，运行时由 LibraryLoader 从 exe 目录加载)
    ↓
utils ←────────────────┴
```

Layout（Windows 构建产物，Linux 去掉 `.dll`/前缀规则一致）：

```
build/bin/
├── cjah.exe
├── cjast_helper_core.dll
├── cjast_wrapper.dll
├── utils.dll
├── cjast_desugar_pass.dll      # Pass 插件
├── cjast_to_source_pass.dll    # Pass 插件
└── config/                     # valid_options.json / passes.json
```

## 阶段管线

cjah 复用前端的 `CompilerInstance`，按 `--dump-source` 指定阶段顺序执行
前端 pipeline（wrapper 的 CangjieFrontendHelper 封装，见 usage.md）：

```
parse → (desugared-parse) → macro → sema → desugared-sema
   │          │               │        │         │
 Parse()  DesugaredParse() MacroExpand() Sema()  DesugaredSema()
   + ConditionCompile()
```

- 每阶段由 stageMap 注册回调驱动，阶段间共享同一 `CompilerInstance`（解析只做一次）；
- `check-syntax` 模式只执行 parse 阶段并上报语法错误，提前结束；
- 分析阶段对每个包运行 PassManager：to-source 等 Pass 以 `ConstAstVisitor` 遍历 AST，
  将（解糖后的）AST 还原输出为源码文件。

## 参数分发

命令行参数在 ArgHelper 中按 `valid_options.json`（`config/valid_options.json`）分拣：

- 匹配工具选项 key 的 → 工具自身处理（阶段、过滤、宏开关、pass 配置等）；
- 其余参数 → 原样透传给前端 `CompilerInvocation::ParseArgs`（如 `-p`、
  `--output-dir`、`--diagnostic-format` 等 cjc 选项）。

## 数据流（时序）

![时序图](./svgs/core_seq.svg)

简要流程：

1. `main` 解析 argv/env，构造 `Options`；
2. `ArgHelper` 按 `valid_options.json` 分拣工具选项与透传参数，初始化 PassManager
   （加载 `passes.json`）；
3. `AstHelper` 构造 `CangjieFrontendHelper`（内部完成前端参数解析）；
4. `Run()`：DoParse（按 stage 执行前端 pipeline）→ （check-syntax 提前结束）→
   DumpAst（可选）→ DoAnalysis（对每个包跑配置的 Pass 链）；
5. 诊断由 DiagnosticEngine 输出（JSON 模式经 JsonDiagCollector 聚合为一份文档）。