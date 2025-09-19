# 架构图

## 组件图

![架构图](./svgs/framework.svg)

### 模块划分

1. utils 工具模块
    1. 功能： 工具类封装
    2. 依赖： 仅依赖标准库

2. wrapper 封装模块
    1. 功能： 封装外部模块 cangjie-fe （动态库）
        1. 包括 AstNode 等接口封装
    2. 依赖：utils

3. core 核心模块， 依赖 utils， wrapper
    1. 功能： 核心模块，实现 AST 树遍历 和 分析 Pass 的核心接口定义
    2. 模块：
        1. visitor 访问者模块
            1. 功能： 封装 AST 节点访问器
            2. 依赖： utils wrapper
        2. pass 模块 
            1. 功能：提供 Pass 接口定义
            2. 依赖： visitor wrapper utils
    3. 依赖： utils wrapper

4. main 模块
    1. 功能： 提供主入口
    2. 依赖： wrapper，core， utils

### 实现架构图

根据项目当前的模块划分和依赖关系，建议采用动态库方式组织项目，以提高模块化程度和可维护性。

#### 动态库划分建议

按照功能职责将项目划分为以下独立的动态库：

1. **libutils.so** - 工具模块
   - 包含：Logger, ArgHelper, Printer 等工具类
   - 依赖：仅依赖标准库

2. **libcjast_wrapper.so** - Cangjie前端封装模块
   - 包含：AstNodeHelper, CangjieFrontendHelper 等封装类
   - 依赖：utils

5. **libcjast_helper_core.so** - 核心模块
   - 包含：AstHelper 等核心功能实现
      - ConstAstVisitor, MutAstVisitor, VisitorBase 等访问者类
      - DesugarPass, ToSourcePass, Pass 等Pass相关类
      - AstHelper 组织主类
   - 依赖：cjast_wrapper, utils

6. **cjah** - 主程序可执行文件
   - 包含：Main.cpp 主入口点
   - 依赖：cjast_helper_core

#### 动态库间依赖关系

```
cjah (可执行文件)
    ↓
libcjast_helper_core.so → libcjast_wrapper.so → libcangjie-fe.so
    ↓                                   
libutils.so ←────────────────────┴
```


## 交互图

![架构图](./svgs/core_seq.svg)
