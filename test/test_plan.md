# CJASTHelper 测试计划

## 1. 目标

本测试计划旨在覆盖所有 Cangjie 编程语言支持的语法场景，确保 CJASTHelper 工具能够正确处理各种语法结构，包括但不限于函数、类、接口、结构体、枚举、泛型等。

## 2. 测试策略

1. 基于现有的 [main.cj](file:///home/tiny/work/code/third_party_cj/CJASTHelper/test/main.cj) 示例文件进行扩展
2. 参考 Cangjie 官方文档中的语法规范
3. 覆盖不同的源码阶段（PARSE, DESUGARED_PARSE, SEMA, DESUGARED_SEMA）
4. 包含声明过滤功能测试
5. 实现命令行参数解析测试

## 3. 语法特性分类及测试场景

### 3.1 包和导入 (Package and Import)

- [x] 基本包声明 (已覆盖)
- [x] 基本导入语句 (已覆盖)
- [x] 通配符导入 (`import std.collection.*`) (已覆盖)
- [x] 别名导入 (`import std.io.InputStream as IS`) (已覆盖)
- [x] 内部导入 (`internal import`) (已覆盖)
- [x] 结构化导入 (`import std.{math.abs, binary.*}`) (已覆盖)

### 3.2 函数 (Functions)

- [x] 基本函数定义 (已覆盖)
- [x] 函数重载 (已覆盖)
- [x] 带注解的函数 (`@C`, `@OverflowThrowing`) (已覆盖)
- [x] 不同访问修饰符的函数 (public, private, internal) (已覆盖)
- [x] 不同修饰符的函数 (unsafe) (已覆盖)
- [x] 参数类型（基本类型、元组、函数类型、可空类型等）(已覆盖)
- [x] 泛型函数 (已覆盖，见foo5)
- [ ] 高阶函数
- [x] Lambda 表达式 (已覆盖)
- [x] 闭包 (已覆盖)

### 3.3 类 (Classes)

- [x] 基本类定义 (已覆盖)
- [x] 继承 (`<: Object`) (已覆盖)
- [x] 构造函数 (`init`) (已覆盖)
- [x] 属性 (var/let) (已覆盖)
- [x] 访问修饰符 (public, private) (已覆盖)
- [x] 方法定义 (已覆盖)
- [x] 运算符重载 (`operator func`) (已覆盖)

### 3.4 接口 (Interfaces)

- [x] 基本接口定义 (已覆盖)
- [x] 接口方法定义 (已覆盖)
- [x] 接口属性定义 (已覆盖)

### 3.5 结构体 (Structs)

- [x] 基本结构体定义 (已覆盖)
- [x] 带访问修饰符的字段 (已覆盖)
- [x] 构造函数 (已覆盖)

### 3.6 枚举 (Enums)

- [x] 基本枚举定义 (已覆盖)
- [x] 带值的枚举项 (已覆盖)
- [x] 枚举实现接口 (已覆盖)

### 3.7 扩展 (Extensions)

- [x] 基本扩展语法 (已覆盖)
- [x] 接口扩展 (已覆盖)

### 3.8 泛型 (Generics)

- [x] 泛型类 (已覆盖)
- [ ] 泛型接口
- [ ] 泛型结构体
- [ ] 泛型枚举
- [x] 泛型约束 (`where` 子句) (已覆盖)
- [x] 类型别名 (`type`) (已覆盖)

### 3.9 模式匹配 (Pattern Matching)

- [x] 基本 match 表达式 (已覆盖)
- [x] Option 类型匹配 (已覆盖)
- [ ] 元组匹配
- [ ] 类型匹配

### 3.10 控制流 (Control Flow)

- [x] if 表达式 (已覆盖)
- [x] while 循环 (已覆盖)
- [x] do-while 循环 (已覆盖)
- [x] for 循环 (范围、容器、字符串) (已覆盖)
- [x] break/continue 语句 (已覆盖)
- [x] if-let 表达式 (已覆盖)

### 3.11 表达式 (Expressions)

- [x] 算术运算符 (已覆盖)
- [x] 逻辑运算符 (已覆盖)
- [x] 比较运算符 (已覆盖)
- [x] 位运算符 (已覆盖)
- [x] 自增自减运算符 (已覆盖)
- [ ] 空合并操作符
- [x] 条件表达式 (已覆盖)

### 3.12 其他特性

- [x] 元组 (已覆盖)
- [x] 数组和集合 (已覆盖)
- [x] 可空类型 (已覆盖)
- [x] 可空可空类型 (`??S`) (已覆盖)
- [x] 属性 (prop) (已覆盖)
- [x] 注解 (已覆盖)

## 4. 测试阶段覆盖

每个语法特性需要在以下阶段进行测试：

- [x] PARSE (已覆盖)
- [x] DESUGARED_PARSE (已覆盖)
- [x] SEMA (已覆盖)
- [x] DESUGARED_SEMA (已覆盖)

## 5. 声明过滤测试

测试以下声明类型的过滤功能：

- [x] func (已覆盖)
- [x] class (已覆盖)
- [x] interface (已覆盖)
- [x] struct (已覆盖)
- [x] enum (已覆盖)
- [x] var (已覆盖)

## 6. 命令行参数测试

- [x] 文件路径参数 (已覆盖)
- [x] 阶段参数 (已覆盖)
- [x] 声明类型过滤参数 (已覆盖)

## 7. 测试文件组织

- [x] test/test_plan.md: 测试计划文档 (已完成)
- [x] test/test_ast_helper.cpp: AST辅助功能测试 (已完成)
- [x] test/test_file.cpp: 文件处理测试 (已完成)
- [x] test/test_example.cpp: 示例测试 (已完成)
- [x] test/test_args.cpp: 命令行参数解析测试 (已完成)
- [ ] test/test_syntax_scenarios.cpp: 各语法场景测试

## 8. 构建和运行

1. 设置环境变量 CANGJIE_INCLUDE 和 CANGJIE_HOME
2. 构建带测试的版本
3. 运行所有测试用例