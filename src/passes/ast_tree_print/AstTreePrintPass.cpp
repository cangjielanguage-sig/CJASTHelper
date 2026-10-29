/**
 * @file
 *
 * This file implements the AstTreePrintPass.
 *
 * 实现自 `wrapper/AstTreePrinter` 迁入：遍历交给 `ConstAstVisitor` 的
 * Before/Visit/After 钩子，缩进与刷新走仓库 `Printer` 的 `Indent()/Unindent()/Flush()`。
 */
#include "AstTreePrintPass.h"

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>

#include "cangjie/AST/AttributePack.h"
#include "cangjie/AST/Node.h"
#include "utils/Cast.h"
#include "wrapper/AstNodeHelper.h"

namespace {
/**
 * @brief 全 kind 表（kind → 打印词）。
 *
 * 两级来源，禁止自造词：
 *   ① `wrapper/AstInfo.inc` 第三字段 —— golden 词面（与 SDK 类名同源，个别词可在此覆盖）；
 *   ② SDK `cangjie/AST/ASTKind.inc` 第三字段（类名）—— **全量兜底**。
 *      为什么需要②：`AstInfo.inc` 同时被 `TypeAlias.h` 当"类型别名清单"用（`using AST::DEF;`），
 *      只能列**真实存在且可别名**的节点（95/114）；区间标记（`DECL`/`EXPR`/`TYPE`/`PATTERN` 等）与
 *      `PERFORM_EXPR`/`RESUME_EXPR`/`IF_AVAILABLE_EXPR`/`DUMMY_BODY`/`FEATURE_*` 共 19 个缺失，
 *      以前这些节点一律打成 `Unknown`（如 sema 生成的 `ArrayExpr.type`）。
 *      用 `emplace` 兜底且不覆盖① ⇒ **任何合法 kind 都有词**，`Unknown` 只剩防御性默认值。
 */
const UnorderedMap<AstKind, const char*>& AllKinds()
{
    static const UnorderedMap<AstKind, const char*> table = [] {
        UnorderedMap<AstKind, const char*> kinds{
#define AST_INFO(KIND, STR, DEF) {AstKind::KIND, #DEF},
#include "wrapper/AstInfo.inc"
#undef AST_INFO
        };
#define ASTKIND(KIND, VALUE, NODE, SIZE) kinds.emplace(AstKind::KIND, #NODE);
#include "cangjie/AST/ASTKind.inc"
#undef ASTKIND
        return kinds;
    }();
    return table;
}

/** @brief kind → 打印词（golden 词面例外集中于此）。 */
const char* KindWord(AstKind kind)
{
    auto& table = AllKinds();
    auto it = table.find(kind);
    return it == table.end() ? "Unknown" : it->second;
}

/** @brief 控制符转义（golden 用字面 \t/\n/\r 写法） */
/** @brief 单字节控制字符 → `\u{X}`（统一口径：除 \t/\n/\r 三个惯用写法外，其余控制字符一律转义）。 */
Str ControlCharEscape(unsigned char c)
{
    static const char* kHex = "0123456789ABCDEF";
    Str out = "\\u{";
    if (c >= 0x10) {
        out += kHex[(c >> 4) & 0xF];
    }
    out += kHex[c & 0xF];
    out += "}";
    return out;
}

/**
 * @brief 文本转义：把不可见字符转成可读形式，避免树文件被写成二进制。
 *
 * 口径（2026-10-27 统一）：
 *   - `\t` / `\n` / `\r` → 惯用转义（与源码写法一致，保持既有基线不变）；
 *   - 其余控制字符（`0x00`–`0x1F`、`0x7F`）→ `\u{X}`（大写十六进制、最短位数）——
 *     原实现只处理三个惯用转义，`r'\0'` 之类会**原样写出 NUL 字节** ⇒
 *     `<pkg>.tree` 变成二进制文件（`grep` 报 "binary file matches"、diff 工具失效）。
 *   - 其余字节原样保留（含 UTF-8 多字节序列，逐字节透传不改动）。
 */
Str EscapeText(ConStr& s)
{
    Str out;
    for (char ch : s) {
        unsigned char c = static_cast<unsigned char>(ch);
        switch (c) {
            case '\t': out += "\\t"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            default:
                if (c < 0x20 || c == 0x7F) {
                    out += ControlCharEscape(c);
                } else {
                    out += ch;
                }
                break;
        }
    }
    return out;
}

/** @brief TypeKind => 语言类型写法：复用编译器 TypeKind.inc 第三字段（禁自造）。 */
Str TypeKindName(TypeKind k)
{
    static const UnorderedMap<TypeKind, const char*> table{
#define TYPE_KIND(KIND, TYPE, NAME) {TypeKind::KIND, NAME},
#include "cangjie/AST/TypeKind.inc"
#undef TYPE_KIND
    };
    auto it = table.find(k);
    return it == table.end() ? "" : it->second;
}

/**
 * @brief 枚举体直接成员**不再**一律记 `Constructor` ✓（修复基线打印 ✗ 用户裁定 2026-10-18 ✓）
 *
 * 旧行为（已删 ✗）：把 `EnumDecl` 体内的 `VarDecl`/`FuncDecl` 统统当构造器词面 ⇒ 函数被印成
 * `Constructor` ✗、prop 的 `get()` 体与函数体语句也被误当构造器 ✗（实证 `option.cj:19:5`、
 * `endian.cj:22:9`/`23:13`、`option.cj:69:9` ✓）。现改为**按真实种类出词面** ✓
 * （枚举项本身即 `Constructor` 节点 ⇒ 自然仍是 `Constructor` ✓）。
 */
bool IsEnumCtorLike(AstKind)
{
    return false;   // 词面不再打折 ✓（保留真实 Kind 与函数名形参 ✓）
}

/** @brief 透明节点：不出行、不进层，仅透传子级（golden 不呈现这些包装层）。 */
bool IsTransparent(AstKind kind)
{
    switch (kind) {
        case AstKind::FUNC_BODY:
        case AstKind::FUNC_PARAM_LIST:
        case AstKind::CLASS_BODY:
        case AstKind::OPTIONAL_CHAIN_EXPR:
        case AstKind::INTERFACE_BODY:
        case AstKind::STRUCT_BODY:
        case AstKind::MODIFIER:
        case AstKind::CONSTANT_TYPE:
        case AstKind::GENERIC: // ★ 形参列表层 ⇒ **透明**（只透传参数子级 ✓ 一参一行 ✓）
            return true;
        default:
            return false;
    }
}

/** @brief **整棵跳过**节点：不出行、不进层、**也不下探**（注解 ⇒ 口径改由声明行属性位承载 ✓）。
 *
 * ★ 与 `IsTransparent` 的区别：透明节点仍**透传子级** ✗；本类**连子级一起跳过** ✓。
 * ★ 缩进账必须**对称**：`AfterNode` 对同一判据必须**不减层** ✗
 *   —— 初版只加了 `BeforeNode` 的 Skip ⇒ 注解的 AfterNode 仍执行 Unindent ⇒
 *   其后所有行**整体退一层** ✗（实测基线里返回类型从 ind 1 退到 ind 0 ✗）。
 */
bool IsSkippedKind(AstKind kind)
{
    return kind == AstKind::ANNOTATION;
}

/** @brief 「行头」节点：出行但不进层（其子级与它同级）——FILE 根与 GenericParam 行。 */
bool IsRowOnlyKind(AstKind kind)
{
    return kind == AstKind::FILE; // 泛型层已并入透明表（一参一行 ✓ 用户口径 ✓）
}

/** @brief 是否为「花括号导入」规格（content.kind == IMPORT_MULTI）。 */
const ImportSpec* AsMultiImportSpec(const AstNode& node)
{
    if (node.astKind != AstKind::IMPORT_SPEC) {
        return nullptr;
    }
    auto& spec = Cast<const ImportSpec&>(node);
    return spec.content.kind == ImportKind::IMPORT_MULTI ? &spec : nullptr;
}

/** @brief 仅前缀文本（IMPORT_MULTI 的 content 行：`std.collection`，不带 `.*`）。 */
Str PrefixOnlyText(const ImportContent& ic)
{
    Str s;
    for (auto& pre : ic.prefixPaths) {
        if (!s.empty()) {
            s += ".";
        }
        s += pre;
    }
    return s;
}

/**
 * @brief 项名 = 该 content 的**源码写法**（去掉 multi 的公共前缀部分）。TC-V3-49 口径 ③ ✓
 *
 * 源码 `import std.overflow.{CheckedOp, WrappingOp}` ⇒ 公共前缀 `std.overflow` 出在**前缀行** ✓，
 * 各项按**源码原样**只写 `CheckedOp` ✓（原实现把公共前缀又拼进项名 ✗ ⇒ 与源码不对应 ✗）。
 * 无公共前缀时（`import {std.io.*, std.fsync.*, …}`）⇒ 原样全出 `std.io.*` ✓。
 */
Str ItemTextWithoutMultiPrefix(const ImportContent& ic, const ImportContent& multi)
{
    Str s;
    const size_t skip = multi.prefixPaths.size();
    for (size_t i = skip; i < ic.prefixPaths.size(); ++i) {
        s += ic.prefixPaths[i] + ".";
    }
    s += ic.identifier.Empty() ? "*" : ic.identifier.Val();
    if (!ic.aliasName.Empty()) {
        s += " as " + ic.aliasName.Val();
    }
    return s;
}

bool IsImportItemOf(const ImportSpec& spec, const ImportSpec& multi)
{
    // ⚠️ 只认 IMPORT_SINGLE ✗ 会把通配项全拒（枚举实测：0=单名 `std.sync.Mutex` ／
    //   2=**通配** `std.io.*` ／ 3=花括号 ⇒ 通配项才是多数 ✓）。故只拒「新的花括号导入」✓。
    if (spec.content.kind == ImportKind::IMPORT_MULTI) {
        return false;
    }
    if (spec.begin.line != multi.begin.line) {
        return false;
    }
    const auto& a = spec.content.prefixPaths;
    const auto& b = multi.content.prefixPaths;
    if (a.size() < b.size()) {
        return false;
    }
    for (size_t i = 0; i < b.size(); ++i) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

/** @brief 该 kind 是否是类型节点（IsExpr/AsExpr 只出表达式面，见 golden 144:13/145:13）。 */
bool IsTypeKindNode(AstKind kind)
{
    switch (kind) {
        case AstKind::PRIMITIVE_TYPE:
        case AstKind::REF_TYPE:
        case AstKind::OPTION_TYPE:
        case AstKind::QUALIFIED_TYPE:
        case AstKind::VARRAY_TYPE:
        case AstKind::CONSTANT_TYPE:
        case AstKind::GENERIC:
            return true;
        default:
            return false;
    }
}

/** @brief 定位文本（缺省包名者位记 `-`）。 */
Str LocText(const AstNode& node)
{
    if (node.astKind == AstKind::PACKAGE_SPEC && Cast<const PackageSpec&>(node).packageName.Empty()) {
        return "-";
    }
    auto& b = node.begin;
    if (b.line == 0 && b.column == 0) {
        return "-";
    }
    std::ostringstream os;
    os << b.line << ":" << b.column;
    return os.str();
}

/** @brief 修饰符尾（按源码位次输出，如 `public static`）。 */
/** @brief 注解**条件文本**（仅 `@When[cond]` 形态；`condExpr` 由解析器生成 ✓ 不走条件编译后才留存 ✓）
 *
 * 覆盖解析器允许的四类（`ParseAnnotations.cpp` 的 when 校验：unary / binary / ref ✓ ＋字面量 ✓）；
 * 其余形态返回占位 `?` —— **不猜** ✗（宁可显式留疑，也不伪造源码事实 ✗）
 */
Str CondText(const Expr& e)
{
    switch (e.astKind) {
        case AstKind::REF_EXPR: {
            auto& r = Cast<const RefExpr&>(e);
            return r.ref.identifier.Val();
        }
        case AstKind::LIT_CONST_EXPR: {
            auto& l = Cast<const LitConstExpr&>(e);
            if (l.kind == Cangjie::AST::LitConstKind::STRING) {
                return "\"" + l.stringValue + "\"";
            }
            return l.stringValue;
        }
        case AstKind::UNARY_EXPR: {
            auto& u = Cast<const UnaryExpr&>(e);
            return Tk2Str(u.op) + (u.expr != nullptr ? CondText(*u.expr) : Str("?"));
        }
        case AstKind::BINARY_EXPR: {
            auto& b = Cast<const BinaryExpr&>(e);
            Str ls = b.leftExpr != nullptr ? CondText(*b.leftExpr) : Str("?");
            Str rs = b.rightExpr != nullptr ? CondText(*b.rightExpr) : Str("?");
            return ls + " " + Tk2Str(b.op) + " " + rs;
        }
        // ★ 括号表达式：递归渲染（TC-V3-49 口径 ② ✓ 见 typechecker/docs/baseline-print-spec.md）
        //   原缺此分支 ⇒ 落 `default: "?"` ✗ ⇒ 印成 `@When[backend == "cjnative" && ?]`，
        //   **整块右操作数被丢弃** ✗（实测 runtime_trace.cj:21 等 4 处 ✓）
        case AstKind::PAREN_EXPR: {
            auto& p = Cast<const ParenExpr&>(e);
            return "(" + (p.expr != nullptr ? CondText(*p.expr) : Str("?")) + ")";
        }
        default:
            return "?";
    }
}

Str ModText(const AstNode& node, bool semaStage)
{
    Str out;
    // ★ 语义属性（用户口径）：sema 阶段把 `Attribute::COMPILER_ADD`（编译器新增 / 解糖生成）
    //   一并打到属性位。非声明节点（解糖表达式等）同样会带该属性 ⇒ 不能再靠下面 `IsDecl()`
    //   早退把它吞掉（那是"属性只在声明行出现"的旧口径）。
    const bool compilerAdded = semaStage && node.TestAttr(Attribute::COMPILER_ADD);
    // ⚠️ 不能只用 `node.IsDecl()` ✗：`VAR_WITH_PATTERN_DECL`（`var _ = …` ／ `var (a, b) = …`）虽
    //   继承 `VarDeclAbstract <: Decl` ✓，却落在 `IsDecl()` 的 `[DECL, INVALID_DECL]` 区间**之外** ✗
    //   ⇒ 本函数早退 ⇒ 它的 `@When[...]` 被**静默丢掉** ✓（实证 `runtime_trace.cj:131`
    //   `@When[…]` ＋ `var _ = atExit(traceStopByConfig)` ⇒ 金印 `VarDecl // 131:1` 无属性 ✗、
    //   我方有 ✓ ⇒ 判基线漏印 ⇒ 改基线 ✓；规格见 typechecker/docs/baseline-print-spec.md ②b ✓）。
    if (!node.IsDecl() && node.astKind != AstKind::VAR_WITH_PATTERN_DECL) {
        return compilerAdded ? Str("COMPILER_ADD") : out;
    }
    auto& decl = Cast<const Decl&>(node);
    // ★★ 属性行**归一字段序**（用户口径 ✓ 两侧一致 ✓）：`位置 ⇒ Modifier ⇒ Annotation ⇒ 其它`
    //   —— **不看源码语序** ✗：修饰符整组在前、注解整组在后 ✓（各组内部仍按源位次 ✓）
    //   例：源码 `@Frozen public const func` ⇒ 归一后 `[public const @Frozen]` ✓
    auto byPos = [](const std::pair<Cangjie::Position, Str>& x, const std::pair<Cangjie::Position, Str>& y) {
        if (x.first.line != y.first.line) {
            return x.first.line < y.first.line;
        }
        return x.first.column < y.first.column;
    };
    std::vector<std::pair<Cangjie::Position, Str>> items; // ① Modifier
    for (auto& m : decl.modifiers) {
        items.emplace_back(m.begin, Tk2Str(m.modifier));
    }
    std::vector<std::pair<Cangjie::Position, Str>> annos; // ② Annotation（同一属性行 ✓ 不单独下钻 ✗）
    for (auto& an : decl.annotations) {
        if (an == nullptr) {
            continue;
        }
        Str at = "@";
        if (an->isCompileTimeVisible) {
            at += "!";
        }
        at += an->identifier.Val();
        // 括号内文本（与 LLT 侧 `toTokens` 同形 ✓）：
        //   ① `@When[cond]` ⇒ `condExpr` ✓（解析期生成 ✓）② `@Deprecated[msg: "..."]` ⇒ `args` ✓
        Str inner;
        if (an->condExpr != nullptr) {
            inner = CondText(*an->condExpr);
        } else {
            for (auto& a : an->args) {
                if (a == nullptr) {
                    continue;
                }
                Str piece;
                Str nm = a->name.Val(); // `SrcIdentifier` 是**值**类型 ✗ 非指针 ✓
                if (!nm.empty()) {      // 具名实参 `message: "..."` ✓
                    piece = nm + ": ";
                }
                if (a->expr != nullptr) {
                    piece += CondText(*a->expr);
                }
                inner += (inner.empty() ? "" : ", ") + piece;
            }
        }
        if (!inner.empty()) {
            at += "[" + inner + "]";
        }
        annos.emplace_back(an->begin, at);
    }
    std::stable_sort(items.begin(), items.end(), byPos);
    std::stable_sort(annos.begin(), annos.end(), byPos);
    items.insert(items.end(), annos.begin(), annos.end());
    for (auto& it : items) {
        if (!out.empty()) {
            out += " ";
        }
        out += it.second;
    }
    // 语义属性排最后（`位置 ⇒ Modifier ⇒ Annotation ⇒ 语义属性`）
    if (compilerAdded) {
        if (!out.empty()) {
            out += " ";
        }
        out += "COMPILER_ADD";
    }
    return out;
}

/** @brief 节点名：按种类取（声明取标识符；类型/字面量/算子各有其面）。 */
Str NameText(const AstNode& node)
{
    switch (node.astKind) {
        case AstKind::PACKAGE_SPEC: {
            auto& spec = Cast<const PackageSpec&>(node);
            if (spec.packageName.Empty()) {
                return "default";
            }
            // ★ 修缺陷（用户裁定 ✓）：原实现只取 `packageName`（**末段** ✗）⇒ `package std.core` 打成 `core` ✗（丢前缀 ✗）
            //   正解：按节点自带的 `prefixPaths`（前缀段 ✓）拼**全名** ⇒ `std.core` ✓
            std::string full;
            for (auto& seg : spec.prefixPaths) {
                full += seg;
                full += ".";
            }
            full += spec.packageName.Val();
            return full;
        }
        case AstKind::PRIMITIVE_TYPE:
            return Cast<const PrimitiveType&>(node).str;
        case AstKind::LIT_CONST_EXPR:
            return EscapeText(Cast<const LitConstExpr&>(node).stringValue);
        case AstKind::BINARY_EXPR:
            return Tk2Str(Cast<const BinaryExpr&>(node).op);
        case AstKind::REF_EXPR:
            return Cast<const RefExpr&>(node).ref.identifier.Val();
        case AstKind::REF_TYPE:
            return Cast<const RefType&>(node).ref.identifier.Val();
        case AstKind::MEMBER_ACCESS:
            return Cast<const MemberAccess&>(node).field.Val();
        case AstKind::OPTION_TYPE:
            return "?";
        case AstKind::JUMP_EXPR:
            return Cast<const JumpExpr&>(node).isBreak ? "break" : "continue";   // golden 实证
        case AstKind::PRIMITIVE_TYPE_EXPR:
            return TypeKindName(Cast<const PrimitiveTypeExpr&>(node).typeKind);   // 复用 TypeKind.inc，零自造
        case AstKind::MACRO_EXPAND_EXPR:
            return Cast<const MacroExpandExpr&>(node).identifier.Empty()
                       ? ""
                       : Cast<const MacroExpandExpr&>(node).identifier.Val();
        case AstKind::QUALIFIED_TYPE:
            return Cast<const QualifiedType&>(node).field.Empty()
                       ? ""
                       : Cast<const QualifiedType&>(node).field.Val();
        case AstKind::VAR_PATTERN:
            return Cast<const VarPattern&>(node).varDecl->identifier.Val();
        case AstKind::VARRAY_TYPE: {
            // golden：`VArrayType $5`（尺寸取自 ConstantType 的字面量）
            auto& vt = Cast<const VArrayType&>(node);
            if (vt.constantType != nullptr && vt.constantType->astKind == AstKind::CONSTANT_TYPE) {
                auto& ct = Cast<const ConstantType&>(*vt.constantType);
                if (ct.constantExpr != nullptr && ct.constantExpr->astKind == AstKind::LIT_CONST_EXPR) {
                    return "$" + Cast<const LitConstExpr&>(*ct.constantExpr).stringValue;
                }
            }
            return "";
        }
        case AstKind::UNARY_EXPR:
            return Tk2Str(Cast<const UnaryExpr&>(node).op);
        case AstKind::INC_OR_DEC_EXPR:
            return Tk2Str(Cast<const IncOrDecExpr&>(node).op);   // golden：`IncOrDecExpr ++ // 213:9`
        case AstKind::RANGE_EXPR:
            // RangeExpr 无 op 字段：由 isClosed 决定 `..=` / `..`
            return Cast<const RangeExpr&>(node).isClosed ? "..=" : "..";
        case AstKind::IMPORT_CONTENT: {
            auto& ic = Cast<const ImportContent&>(node);
            Str s;
            for (auto& pre : ic.prefixPaths) {
                s += pre + ".";
            }
            s += ic.identifier.Empty() ? "*" : ic.identifier.Val();
            if (!ic.aliasName.Empty()) {
                s += " as " + ic.aliasName.Val();
            }
            return s;
        }
        default:
            break;
    }
    if (!node.IsDecl()) {
        return "";
    }
    auto& decl = Cast<const Decl&>(node);
    return decl.identifier.Empty() ? "" : decl.identifier.Val();
}
} // namespace

const char* AstTreePrintPass::WordOf(const AstNode& node) const
{
    if (inEnumBody_ && IsEnumCtorLike(node.astKind)) {
        return "Constructor";
    }
    if (node.astKind == AstKind::ARRAY_EXPR && Cast<const Cangjie::AST::ArrayExpr&>(node).isValueArray) {
        return "VArrayExpr";     // golden：值数组表达式
    }
    if (inNestedImport_ && node.astKind == AstKind::IMPORT_SPEC) {
        return "ImportContent";   // golden：嵌套导入条目
    }
    return KindWord(node.astKind);
}

bool AstTreePrintPass::IsCtorContext(const AstNode& node) const
{
    return inEnumBody_ && IsEnumCtorLike(node.astKind);
}

bool AstTreePrintPass::IsTransparentKind(const AstNode& node) const
{
    // 修复基线打印 ✗（用户裁定 2026-10-18 ✓）：旧版在枚举体内把 `FUNC_PARAM` 视作透明 ⇒
    //   丢形参名 ✗（`option.cj:30:28 exception` 等 6 处 ✓）；现**与其它上下文一致**出 `FuncParam <名>` 行 ✓。
    return IsTransparent(node.astKind);
}

VisitResult AstTreePrintPass::BeforeNode(const AstNode& node)
{
    // ★ 整棵跳过（注解 ✓ 不出行、不进层、不下探 ⇒ 口径改由声明行属性位承载 ✓ 见 `ModText`）
    if (IsSkippedKind(node.astKind)) {
        return VisitResult::Skip();
    }
    if (!IsTransparentKind(node)) {
        PRT().PVal(WordOf(node)).PVal(Detail(node, AtTop(node))).PNL();
        if (!IsRowOnlyKind(node.astKind)) {
            PRT().Indent();
        }
    }
    return VisitResult::Cont();
}

void AstTreePrintPass::AfterNode(const AstNode& node)
{
    // ★ 整棵跳过节点不进层 ⇒ **亦不减层** ✓（缩进账与 BeforeNode 对称 ✗ 初版漏此 ⇒ 其后整段退一层 ✗）
    if (IsSkippedKind(node.astKind)) {
        return;
    }
    // 透明节点未进层 ⇒ 亦不减层；行头节点（根/泛参行）不进层 ⇒ 亦不减层
    if (!IsTransparentKind(node) && !IsRowOnlyKind(node.astKind)) {
        PRT().Unindent();
    }
    PRT().Flush();
}

Str AstTreePrintPass::Detail(const AstNode& node, bool topLevel)
{
    // ★ 泛型形参：**一参一行**（用户口径 ✓）—— 原在此拼接为单行 ✗，现由参数节点各自出行 ✓
    //   缩进：参数是其宿主的**子节点** ⇒ 比宿主深一级 ✓（用户口径「子节点要有缩进」✓）
    if (node.astKind == AstKind::VAR_WITH_PATTERN_DECL) {
        // golden：VarDecl 行不报名（名在模式侧）✓；**但属性/修饰符仍要印** ✓（TC-V3-49 口径 ②b）
        //   ⚠️ 本分支原直接 return ⇒ 走不到下方 `ModText` ⇒ `var _ = …` 上的 `@When[...]` 被静默丢掉 ✗
        //   （实证 runtime_trace.cj:131：金 `VarDecl // 131:1` 无属性 ✗、我方有 ✓ ⇒ 判基线漏印 ⇒ 改基线 ✓）
        Str mods = ModText(node, Config().Sema());
        return " // " + LocText(node) + (mods.empty() ? Str("") : (" " + mods)) + TySuffix(node) + RefSuffix(node);
    }
    if (this->IsCtorContext(node)) {
        // golden：Constructor 行不出名（只有词面 ＋ 定位）
        return " // " + LocText(node) + TySuffix(node) + RefSuffix(node);
    }
    if (node.astKind == AstKind::FILE) {
        // 根行口径：`Program // <文件名> `（不定位，名后一空格）
        return " // " + Cast<const File&>(node).fileName + " ";
    }
    Str name = NameText(node);
    Str out;
    if (!name.empty()) {
        out += " " + name;
        out += " ";
    } else {
        out += " ";   // 无名行同口径（golden：ImportList     // 2:1）
    }
    out += "// " + LocText(node);
    if (node.astKind == AstKind::IMPORT_SPEC) {
        // golden：导入修饰符来自 ImportSpec::modifier（单数，官方 ToCangjiePass 同源）且**紧跟定位、不插空格**
        auto& sp = Cast<const ImportSpec&>(node);
        if (sp.modifier != nullptr) {
            out += " " + Tk2Str(sp.modifier->modifier);   // 与其余行同口径：定位与修饰符间留一空格
        }
        return out + TySuffix(node) + RefSuffix(node);
    }
    Str mods = ModText(node, Config().Sema());
    if (!mods.empty()) {
        out += " " + mods;
    }
    return out + TySuffix(node) + RefSuffix(node);
}

void AstTreePrintPass::WalkChildren(const AstNode& node)
{
    bool saved = parentIsFile_;
    parentIsFile_ = false;      // 仅 File 的直接子级算"顶层"（留白 5）
    for (auto& child : AstNodeHelper::GetChildren(node)) {
        // ★ 字符串字面量的 `ref` 是 sema 期**编译器补的脚手架** RefType（`Sema/Collector.cpp`
        //   `CollectLitConstExpr()`：仅 STRING/JSTRING，带 `Attribute::COMPILER_ADD | IN_CORE`，
        //   位置与字面量相同）⇒ 打印期隐藏，否则每个字符串字面量都会多出一行与行尾
        //   `type=std.core.String` 完全重复的 `RefType String`。
        //   ✗ 不能改收集器去掉它：`Wrapper.cpp` 的 ReplaceFunc 按 `[ref, siExpr]` 两个槽位
        //     顺序消费 children，收集器少收一个会让 `ReplaceNode` 越界解引用（实测 SIGSEGV，
        //     `OwnedPtr::operator bool(this=0x0)`）⇒ 只能在打印期过滤。
        if (node.astKind == AstKind::LIT_CONST_EXPR && child->astKind == AstKind::REF_TYPE &&
            child->TestAttr(Attribute::COMPILER_ADD)) {
            continue;
        }
        Traverse(*child, visitor);
    }
    parentIsFile_ = saved;
}

void AstTreePrintPass::AppendTypeAnnotation(const AstNode& node)
{
    // 类型标注：仓内收集器未纳入 Decl 类型面 ⇒ 打印期自补（golden 有 PrimitiveType/RefType 标注行）
    // 去重：收集器若已含该指针 ⇒ 不重复补访（避免 PrimitiveType/RefType 过产）
    const Type* ty = nullptr;
    if (node.astKind == AstKind::VAR_DECL) {
        ty = Cast<const VarDecl&>(node).type.get();
    } else if (node.astKind == AstKind::FUNC_PARAM) {
        ty = Cast<const FuncParam&>(node).type.get();
    } else {
        ty = Cast<const PropDecl&>(node).type.get();
    }
    bool already = false;
    for (auto& c : AstNodeHelper::GetChildren(node)) {
        if (c == static_cast<const AstNode*>(ty)) {
            already = true;
            break;
        }
    }
    if (ty != nullptr && !already) {
        Traverse(*ty, visitor);
    }
}

Str AstTreePrintPass::TyText(const Cangjie::AST::Ty* ty)
{
    using namespace Cangjie::AST;
    if (ty == nullptr || ty->IsInvalid()) {
        return Str("");
    }
    const auto arg = [this, ty](size_t i) -> Str {
        return i < ty->typeArgs.size() && ty->typeArgs[i] != nullptr ? TyText(ty->typeArgs[i]) : Str("?");
    };
    const auto nominal = [](const Decl* d) -> Str {
        if (d == nullptr) {
            return Str("?");
        }
        return d->fullPackageName.empty() ? Str(d->identifier) : (d->fullPackageName + "." + d->identifier);
    };
    switch (ty->kind) {
        case TypeKind::TYPE_ARRAY: {
            // RawArray（内建）与 Array（std.core 定义）语义上都是 `Array<T>`；维度 >1 出 `Array<T, $N>`
            auto& at = *static_cast<const ArrayTy*>(ty);
            Str e = arg(0);
            return at.dims > 1 ? ("Array<" + e + ", $" + std::to_string(at.dims) + ">") : ("Array<" + e + ">");
        }
        case TypeKind::TYPE_VARRAY: {
            auto& vt = *static_cast<const VArrayTy*>(ty);
            return "VArray<" + arg(0) + ", $" + std::to_string(vt.size) + ">";
        }
        case TypeKind::TYPE_POINTER:
            return "CPointer<" + arg(0) + ">";
        case TypeKind::TYPE_CSTRING:
            return Str("CString");
        case TypeKind::TYPE_TUPLE: {
            Str out = "(";
            for (size_t i = 0; i < ty->typeArgs.size(); ++i) {
                if (i != 0) {
                    out += ", ";
                }
                out += arg(i);
            }
            return out + ")";
        }
        case TypeKind::TYPE_FUNC: {
            auto& ft = *static_cast<const FuncTy*>(ty);
            Str out = "(";
            for (size_t i = 0; i < ft.paramTys.size(); ++i) {
                if (i != 0) {
                    out += ", ";
                }
                out += TyText(ft.paramTys[i]);
            }
            out += ")->";
            out += TyText(ft.retTy);
            return out;
        }
        case TypeKind::TYPE_CLASS: {
            auto& ct = *static_cast<const ClassTy*>(ty);
            Str n = nominal(ct.decl);
            if (ty->typeArgs.empty()) {
                return n;
            }
            Str out = n + "<";
            for (size_t i = 0; i < ty->typeArgs.size(); ++i) {
                if (i != 0) {
                    out += ", ";
                }
                out += arg(i);
            }
            return out + ">";
        }
        case TypeKind::TYPE_INTERFACE: {
            auto& it = *static_cast<const InterfaceTy*>(ty);
            Str n = nominal(it.decl);
            if (ty->typeArgs.empty()) {
                return n;
            }
            Str out = n + "<";
            for (size_t i = 0; i < ty->typeArgs.size(); ++i) {
                if (i != 0) {
                    out += ", ";
                }
                out += arg(i);
            }
            return out + ">";
        }
        case TypeKind::TYPE_STRUCT: {
            auto& st = *static_cast<const StructTy*>(ty);
            Str n = nominal(st.decl);
            if (ty->typeArgs.empty()) {
                return n;
            }
            Str out = n + "<";
            for (size_t i = 0; i < ty->typeArgs.size(); ++i) {
                if (i != 0) {
                    out += ", ";
                }
                out += arg(i);
            }
            return out + ">";
        }
        case TypeKind::TYPE_ENUM: {
            auto& et = *static_cast<const EnumTy*>(ty);
            Str n = nominal(et.decl);
            if (ty->typeArgs.empty()) {
                return n;
            }
            Str out = n + "<";
            for (size_t i = 0; i < ty->typeArgs.size(); ++i) {
                if (i != 0) {
                    out += ", ";
                }
                out += arg(i);
            }
            return out + ">";
        }
        case TypeKind::TYPE:
            return nominal(static_cast<const TypeAliasTy*>(ty)->declPtr);
        case TypeKind::TYPE_GENERICS:
            return Str(ty->name);   // 形参 `T`（`Generics-T` 是唯一名 ✗）
        default:
            return Str(ty->String());
    }
}

/**
 * @brief 是否**局部声明**（函数体/表达式体内定义）——局部名不进全名（用户口径）。
 *
 * 判据：`outerDecl` 链上先遇到函数/契约体 ⇒ 局部；先遇到类/接口/结构/扩展/枚举 ⇒ 成员。
 * 局部声明没有包级全名（`default::a` 是伪全名），故只显示标识符。
 */
bool IsLocalDeclOf(const Decl& decl)
{
    for (Ptr<Decl> outer = decl.outerDecl; outer != nullptr; outer = outer->outerDecl) {
        if (outer->IsNominalDecl()) {
            return false;   // 成员声明：包名 ＋ 类型限定名
        }
        if (outer->astKind == AstKind::FUNC_DECL || outer->astKind == AstKind::FUNC_BODY ||
            outer->astKind == AstKind::LAMBDA_EXPR || outer->astKind == AstKind::MACRO_DECL) {
            return true;    // 形参、局部 func/闭包内声明
        }
    }
    // 链到头（`outerDecl == nullptr`）时靠语义属性区分（实测：
    //   包级 `func println`/`let gv` ⇒ `Attribute::GLOBAL` ✓；
    //   函数体内的 `let a`／模式变量 ⇒ 无 GLOBAL、非成员 ✗ ⇒ 局部）
    return !decl.IsNominalDecl() && !decl.IsMemberDecl() && !decl.TestAttr(Attribute::GLOBAL);
}

/**
 * @brief 绑定目标的显示名（用户口径：统一 `ref=<fullname>`，局部定义不出全名）。
 *
 * 全名 = `<fullPackageName>::<外层声明标识符.…>.<标识符>`；局部声明只给标识符。
 * 只给名字，暂不带声明 kind/位置（后续按需补）。
 */
Str RefTargetName(const Decl& decl)
{
    Str name = decl.identifier.Val();
    if (name.empty() || IsLocalDeclOf(decl)) {
        return name;
    }
    Str qualified = name;
    for (Ptr<Decl> outer = decl.outerDecl; outer != nullptr; outer = outer->outerDecl) {
        Str id = outer->identifier.Val();
        if (!id.empty()) {
            qualified = id + "." + qualified;
        }
    }
    return decl.fullPackageName.empty() ? qualified : (decl.fullPackageName + "::" + qualified);
}

Str AstTreePrintPass::TySuffix(const AstNode& node)
{
    // ★ 结构化 AST ＋ 语义类型一次成型（含解糖节点 ✓）：`type=<用户面名字>`
    //   parse 阶段 `ty` 是 `Ty::GetInitialTy()`（无效）⇒ 不出后缀 ✓（既有 golden 不受影响）
    Str t = TyText(node.GetTy());
    return t.empty() ? Str("") : (" type=" + t);
}

Str AstTreePrintPass::RefSuffix(const AstNode& node)
{
    // ★ 引用的**绑定结果**（用户口径：统一 `ref=<fullname>`）：只打名字，不带声明 kind/位置。
    //   仅在语义阶段有效：parse 阶段这些字段都还没填 ⇒ 天然不出 ✓。
    //   覆盖面：`RefExpr.ref.target`／`MemberAccess.target`／`CallExpr.resolvedFunction`，
    //   以及**类型节点**里的绑定目标：`RefType.ref.target`／`QualifiedType.target`。
    //   重载集合（`ref.targets` / `MemberAccess.targets`）没有唯一目标 ⇒ 暂不打（后续按需补）。
    if (!Config().Sema()) {
        return Str("");
    }
    Ptr<const Decl> target = nullptr;
    switch (node.astKind) {
        case AstKind::REF_EXPR:
            target = Cast<const RefExpr&>(node).ref.target;
            break;
        case AstKind::MEMBER_ACCESS:
            target = Cast<const MemberAccess&>(node).target;
            break;
        case AstKind::CALL_EXPR:
            target = Cast<const CallExpr&>(node).resolvedFunction;
            break;
        case AstKind::REF_TYPE:
            target = Cast<const RefType&>(node).ref.target;
            break;
        case AstKind::QUALIFIED_TYPE:
            target = Cast<const QualifiedType&>(node).target;
            break;
        default:
            break;
    }
    if (target == nullptr) {
        return Str("");
    }
    Str name = RefTargetName(*target);
    return name.empty() ? Str("") : (" ref=" + name);
}

void AstTreePrintPass::AppendMemberTypeArgs(const AstNode& node)
{
    // 类型实参（MemberAccess.typeArguments）：收集器未纳入 ⇒ 自补下降（带指针去重）
    auto& ma = Cast<const MemberAccess&>(node);
    for (auto& ta : ma.typeArguments) {
        if (ta == nullptr) {
            continue;
        }
        bool already = false;
        for (auto& c : AstNodeHelper::GetChildren(node)) {
            if (c == static_cast<const AstNode*>(ta.get())) {
                already = true;
                break;
            }
        }
        if (!already) {
            Traverse(*ta.get(), visitor);
        }
    }
}

void AstTreePrintPass::Visit(const AstNode& node)
{
    // ImportSpec.content：仓内收集器未纳入 ⇒ 打印期自补（golden 有 ImportContent 族）
    Traverse(Cast<const ImportSpec&>(node).content, visitor);
}

void AstTreePrintPass::RegisterHandlers()
{
    // ① 通用默认：Before 出行＋进层 / Visit 常规子级 / After 减层＋刷新
    //    （经 ConstAstVisitor::RegDefault* 注册；未按种类注册的节点一律走此三钩子）
    visitor.RegDefaultBefore([this](const AstNode& n) { return this->BeforeNode(n); });
    visitor.RegDefaultVisit([this](const AstNode& n, VisitResult& res) {
        this->WalkChildren(n);
        res = VisitResult::Cont();
    });
    visitor.RegDefaultAfter([this](const AstNode& n, const VisitResult&) { this->AfterNode(n); });

    // ② File：缺省包头补行 ＋ 直接子级为顶层
    visitor.RegVisit(AstKind::FILE, [this](const AstNode& n, VisitResult& res) {
        Visit(Cast<const File&>(n), res);
    });

    // ③ FuncBody：golden 子级序 返回类型 => 形参列表 => 函数体
    visitor.RegVisit(AstKind::FUNC_BODY, [this](const AstNode& n, VisitResult& res) {
        Visit(Cast<const FuncBody&>(n), res);
    });

    // ④ LambdaExpr：体内 Block 不出行（直接出表达式）
    visitor.RegVisit(AstKind::LAMBDA_EXPR, [this](const AstNode& n, VisitResult& res) {
        Visit(Cast<const LambdaExpr&>(n), res);
    });

    // ⑤ TypeConvExpr：golden 序 表达式 => 类型
    visitor.RegVisit(AstKind::TYPE_CONV_EXPR, [this](const AstNode& n, VisitResult& res) {
        Visit(Cast<const TypeConvExpr&>(n), res);
    });

    // ⑤b IsExpr/AsExpr：golden 只出表达式、不出类型（`x is Int64` ⇒ IsExpr ⇒ RefExpr x）
    visitor.RegVisit(AstKind::IS_EXPR, [this](const AstNode& n, VisitResult& res) { Visit(Cast<const IsExpr&>(n), res); });
    visitor.RegVisit(AstKind::AS_EXPR, [this](const AstNode& n, VisitResult& res) { Visit(Cast<const AsExpr&>(n), res); });

    // ⑥ ArrayExpr：值数组只出实参，不出类型子树
    visitor.RegVisit(AstKind::ARRAY_EXPR, [this](const AstNode& n, VisitResult& res) {
        Visit(Cast<const ArrayExpr&>(n), res);
    });

    // ⑦ VarWithPatternDecl：`let _ = e`（通配模式）不出模式行，直接出初始化式
    visitor.RegVisit(AstKind::VAR_WITH_PATTERN_DECL, [this](const AstNode& n, VisitResult& res) {
        Visit(Cast<const VarWithPatternDecl&>(n), res);
    });

    // ⑧ VarPattern：不出内层 VarDecl 行，直接出其子级
    visitor.RegVisit(AstKind::VAR_PATTERN, [this](const AstNode& n, VisitResult& res) {
        Visit(Cast<const VarPattern&>(n), res);
    });

    // ⑨ 叶模式：golden 实证「TuplePattern 下不出现元素行」⇒ TuplePattern 作叶（不下探）；
    //    EnumPattern 则**要**下探（golden 18:14 下出 MemberAccess/RefExpr）；VarOrEnumPattern 走常规下探。
    visitor.RegVisit(AstKind::TUPLE_PATTERN, [this](const AstNode& n, VisitResult& res) { Visit(Cast<const TuplePattern&>(n), res); });
    visitor.RegVisit(AstKind::CONST_PATTERN, [this](const AstNode& n, VisitResult& res) { Visit(Cast<const ConstPattern&>(n), res); });

    // ⑨b LetPatternExpr / ForInExpr：golden 不出模式子级（229:9 ⇒ RefExpr o；218:5 ⇒ RefExpr arr）
    visitor.RegVisit(AstKind::LET_PATTERN_DESTRUCTOR, [this](const AstNode& n, VisitResult& res) { Visit(Cast<const LetPatternDestructor&>(n), res); });
    visitor.RegVisit(AstKind::FOR_IN_EXPR, [this](const AstNode& n, VisitResult& res) { Visit(Cast<const ForInExpr&>(n), res); });

    // ⑩ ImportSpec：子级为嵌套条目（词面 ImportContent）＋ content 自补
    visitor.RegVisit(AstKind::IMPORT_SPEC, [this](const AstNode& n, VisitResult& res) {
        Visit(Cast<const ImportSpec&>(n), res);
    });

    // ⑪ EnumDecl：直接子级为构造符语境
    visitor.RegVisit(AstKind::ENUM_DECL, [this](const AstNode& n, VisitResult& res) {
        Visit(Cast<const EnumDecl&>(n), res);
    });

    // ⑫ 类型标注自补（VarDecl / FuncParam）：golden 中类型排在子级之后
    visitor.RegVisit(AstKind::VAR_DECL, [this](const AstNode& n, VisitResult& res) { Visit(Cast<const VarDecl&>(n), res); });
    visitor.RegVisit(AstKind::FUNC_PARAM, [this](const AstNode& n, VisitResult& res) { Visit(Cast<const FuncParam&>(n), res); });

    // ⑭ PropDecl：golden 序＝类型在先、访问器（get/set）在后（见 golden `PropDecl sum // 59:5 public` 邻域）
    visitor.RegVisit(AstKind::PROP_DECL, [this](const AstNode& n, VisitResult& res) {
        Visit(Cast<const PropDecl&>(n), res);
    });

    // ⑫b VArrayType：只下探元素类型（跳过 constantType ⇒ 尺寸已由词面 `$5` 表达，golden 无该行）
    visitor.RegVisit(AstKind::VARRAY_TYPE, [this](const AstNode& n, VisitResult& res) {
        Visit(Cast<const VArrayType&>(n), res);
    });

    // ⑮ GenericConstraint：golden 只出上界（左端类型不出 ⇒ `T <: Base` ⇒ 仅 `RefType Base`）
    // ★ `TryExpr`：按**源码序**成对下探 ✓（未注册则不会被分派 ⇒ 走了默认的槽位序 ✗ —— 首版即踩此坑 ✓）
    visitor.RegVisit(AstKind::TRY_EXPR, [this](const AstNode& n, VisitResult& res) {
        Visit(Cast<const TryExpr&>(n), res);
    });

    visitor.RegVisit(AstKind::GENERIC_CONSTRAINT, [this](const AstNode& n, VisitResult& res) {
        Visit(Cast<const GenericConstraint&>(n), res);
    });

    // ⑬ MemberAccess：类型实参自补
    visitor.RegVisit(AstKind::MEMBER_ACCESS, [this](const AstNode& n, VisitResult& res) {
        Visit(Cast<const MemberAccess&>(n), res);
    });
}

// ───────────────────────── 具名 Visit（照 to_source/DesugarPass 的样式：薄 lambda 转发至此）─────────────

void AstTreePrintPass::Visit(const File& node, VisitResult& res)
{
    bool hasSpec = false;
    for (auto& c : AstNodeHelper::GetChildren(node)) {
        if (c != nullptr && c->astKind == AstKind::PACKAGE_SPEC) {
            hasSpec = true;
            break;
        }
    }
    if (!hasSpec) {
        PRT().PVal("PackageHeader default // -").PNL();   // 留白 1（与统一口径一致）
    }
    bool saved = parentIsFile_;
    parentIsFile_ = true;
    WalkFileChildren(node);
    parentIsFile_ = saved;
    res = VisitResult::Cont();
}

// 同一实现的多 kind 重载：各自转发到共享的私有遍历助手
void AstTreePrintPass::Visit(const IsExpr& node, VisitResult& res)
{
    TraverseTypeTestChildren(node, res);
}
void AstTreePrintPass::Visit(const AsExpr& node, VisitResult& res)
{
    TraverseTypeTestChildren(node, res);
}
void AstTreePrintPass::Visit(const LetPatternDestructor& node, VisitResult& res)
{
    TraversePatternExprChildren(node, res);
}
/**
 * @brief `ForInExpr`：子级按**源码序** ✓（TC-V3-49，2026-10-18）
 *
 * 源码形 `for (p in <range> where <guard>) { … }` ⇒ **range 在前、where 在后** ✓。
 * 旧行为沿 `FOR_IN_EXPR` 收集器序（`patternGuard` 先于 `inExpression` ✗）⇒ 打出
 * `BinaryExpr <`（where）**先于** `RangeExpr` ✗。实证 `argopt/arg_opt.cj:323`：
 * `for (j in 0..long where j + i < size)` ⇒ 我方按源码序、基线反序 ✓ ⇒ 判定
 * **我方更准确** ⇒ 改基线 ✓（同 `TryExpr` 一例 ✓）。模式子级仍不出行 ✓（golden 口径 ✓）。
 */
void AstTreePrintPass::Visit(const ForInExpr& node, VisitResult& res)
{
    bool saved = parentIsFile_;
    parentIsFile_ = false;
    // ⓪ **模式**（`for (p in …)` 源码序在 range 之前 ⇒ 先出 pattern 子树 ✓）
    //    基线旧口径「模式子级不出行」已按用户裁定废止（2026-10-21：基线与对齐都改）
    if (node.pattern != nullptr) {
        Traverse(*node.pattern, visitor);
    }
    if (node.inExpression != nullptr) {
        Traverse(*node.inExpression, visitor); // ① range（`0..long` ✓）
    }
    if (node.patternGuard != nullptr) {
        Traverse(*node.patternGuard, visitor); // ② where 守卫（`j + i < size` ✓）
    }
    if (node.body != nullptr) {
        Traverse(*node.body, visitor);         // ③ body
    }
    parentIsFile_ = saved;
    res = VisitResult::Cont();
}
void AstTreePrintPass::Visit(const VarDecl& node, VisitResult& res)
{
    TraverseTypeAnnotationLast(node, res);
}
void AstTreePrintPass::Visit(const FuncParam& node, VisitResult& res)
{
    TraverseTypeAnnotationLast(node, res);
}
void AstTreePrintPass::Visit(const TuplePattern& node, VisitResult& res)
{
    // ★ 元素模式**照常出行**：原实现把 TuplePattern 当叶 ⇒ `let (a, b) = (1, 2)` 只剩一行空的
    //   `TuplePattern`，绑定项（`VarPattern a` / `VarPattern b`）与元素内类型全丢 ✗。
    //   收集器 `TUPLE_PATTERN ⇒ pat.patterns` 已备好子级，此处按序下探即可。
    bool saved = parentIsFile_;
    parentIsFile_ = false;
    for (auto& pattern : node.patterns) {
        if (pattern != nullptr) {
            Traverse(*pattern, visitor);
        }
    }
    parentIsFile_ = saved;
    res = VisitResult::Cont();
}
void AstTreePrintPass::Visit(const ConstPattern& node, VisitResult& res)
{
    (void)node;
    res = VisitResult::Cont();   // 叶：不下探
}

void AstTreePrintPass::WalkFileChildren(const AstNode& fileNode)
{
    // 一次性取子级指针序列 ⇒ 便于索引扫描（花括号导入需向后吸收兄弟）
    std::vector<const AstNode*> kids;
    for (auto& c : AstNodeHelper::GetChildren(fileNode)) {
        // ★ 内建声明（BuiltInDecl）不属该文件源码：前端 sema 把它们硬塞进 pkg->files[0]->decls，
        //   这里统一跳过，改由 PrintBuiltinDecls() 收在文件块之后输出（见该函数注释）。
        if (c != nullptr && c->astKind != AstKind::BUILTIN_DECL) {
            kids.emplace_back(&*c);
        }
    }
    for (size_t i = 0; i < kids.size(); ++i) {
        const AstNode* cur = kids[i];
        const ImportSpec* multi = AsMultiImportSpec(*cur);
        if (multi == nullptr) {
            parentIsFile_ = true;
            Traverse(*cur, visitor);
            continue;
        }
        // ★ 花括号导入（IMPORT_MULTI）：AST 里是「MULTI + 其后同前缀的 IMPORT_SINGLE 兄弟」，
        //   golden 印成嵌套：ImportList 行 ⇒ content 行（仅前缀）⇒ 缩进一层出各兄弟为 ImportContent 行。
        parentIsFile_ = true;
        EmitImportRow(*cur, "");
        parentIsFile_ = false;
        PRT().Indent();
        // 花括号前缀行：有公共前缀 ⇒ 印前缀 ✓；无（`import {a.*, b.*}`）⇒ **不印名** ✓
        // ⚠️ 不能退回 `NameText` ✗（会把空前缀印成 `*` ✗）；也不能不印前缀 ✗（源码里前缀在花括号前 ✓）
        Str bracePrefix = PrefixOnlyText(multi->content);
        if (bracePrefix.empty()) {
            PRT().PVal(WordOf(multi->content)).PVal(" ").PVal("// " + LocText(multi->content)).PNL();
        } else {
            EmitImportRow(multi->content, bracePrefix);
        }
        PRT().Indent();
        while (i + 1 < kids.size()) {
            const AstNode* nx = kids[i + 1];
            if (nx->astKind != AstKind::IMPORT_SPEC) {
                break;
            }
            auto& nspec = Cast<const ImportSpec&>(*nx);
            if (!IsImportItemOf(nspec, *multi)) {
                break;
            }
            // 项名 = 源码写法（去掉 multi 公共前缀 ✓）：`CheckedOp` ✓ ／ 无前缀时 `std.io.*` ✓
            EmitImportRow(nspec.content, ItemTextWithoutMultiPrefix(nspec.content, multi->content),
                          LocText(nspec));
            ++i;   // 吸收该兄弟
        }
        PRT().Unindent();
        PRT().Unindent();
    }
    parentIsFile_ = true;
}

void AstTreePrintPass::EmitImportRow(const AstNode& node, const Str& nameOverride, const Str& locOverride)
{
    // 与 Detail 的名分支同口径：`词 + 空格 + 名 + 留白 + // line:col`
    // locOverride 非空时用它作定位（重分组时兄弟行的定位取其 ImportSpec 自身，而词/名来自 content）
    Str name = nameOverride.empty() ? NameText(node) : nameOverride;
    Str out;
    if (!name.empty()) {
        out += " " + name;
    }
    out += " ";
    out += "// " + (locOverride.empty() ? LocText(node) : locOverride);
    PRT().PVal(WordOf(node)).PVal(out).PNL();
}

void AstTreePrintPass::EmitEmptyBodyBlock()
{
    // 合成行：无定位的空函数体（golden `        Block // -`，留白取嵌套口径）
    PRT().PVal("Block").PVal(" // -").PNL();
}

void AstTreePrintPass::Visit(const GenericConstraint& node, VisitResult& res)
{
    // 约束的子级为「左端类型 ＋ 上界」⇒ **全量下探** ✓（修复基线打印 ✗ 用户裁定 2026-10-18 ✓：
    //   旧实现只取**末子级** ⇒ `where T <: A & B & C` 只出 `C` ✗ ⇒ 丢约束信息 ✗；
    //   现要求「多上界都打印」✓ —— 左端 `T` 与每个上界各出一行 ✓。）
    for (auto& c : AstNodeHelper::GetChildren(node)) {
        if (c != nullptr) {
            Traverse(*c, visitor);
        }
    }
    res = VisitResult::Cont();
}

/**
 * @brief `TryExpr`：按**源码序**下探 ✓（修复基线打印 ✗ 用户裁定 2026-10-18 ✓）
 *
 * 旧行为 ✗：沿用 `GetChildren` 的**槽位序**（AST 里 `catchPatterns[]` 与 `catchBlocks[]` 是
 * **两个数组** ⇒ 先收全部模式、再收全部体 ✓）⇒ 打印出来 `catch` 的模式与其**体分离** ✗：
 * ```
 *   ExceptTypePattern 67:30 / ExceptTypePattern 69:30 / Block 67:44 / Block 69:40   ← 模式全前 ✗
 * ```
 * 而源码是逐对书写的 ✓（实证 `atexit.cj:65-70`）⇒ 现按 `catchPatterns[i] → catchBlocks[i]`
 * **成对**下探 ✓，与其余节点（`if`/`while`/`for`/`match`）的源码序口径**一致** ✓。
 */
void AstTreePrintPass::Visit(const TryExpr& node, VisitResult& res)
{
    for (auto& r : node.resourceSpec) { // `resourceSpec` 为**向量**（`try (…resources…)` ✓）
        if (r != nullptr) {
            Traverse(*r, visitor);
        }
    }
    if (node.tryBlock != nullptr) {
        Traverse(*node.tryBlock, visitor);
    }
    const size_t np = node.catchPatterns.size();
    const size_t nb = node.catchBlocks.size();
    const size_t n = np < nb ? np : nb;
    for (size_t i = 0; i < n; ++i) {
        Traverse(*node.catchPatterns[i], visitor);
        Traverse(*node.catchBlocks[i], visitor);
    }
    for (size_t i = n; i < np; ++i) { // 数量不齐的异常情形 ⇒ 不丢 ✓
        Traverse(*node.catchPatterns[i], visitor);
    }
    for (size_t i = n; i < nb; ++i) {
        Traverse(*node.catchBlocks[i], visitor);
    }
    if (node.finallyBlock != nullptr) {
        Traverse(*node.finallyBlock, visitor);
    }
    res = VisitResult::Cont();
}

void AstTreePrintPass::Visit(const ImportContent& node, VisitResult& res)
{
    // content 行由 ImportSpec 处自补调用（收集器未纳入）⇒ 此处直接下探该 content 节点
    Traverse(node, visitor);
    res = VisitResult::Cont();
}

void AstTreePrintPass::Visit(const FuncBody& node, VisitResult& res)
{
    bool saved = parentIsFile_;
    parentIsFile_ = false;
    if (node.generic != nullptr) {   // golden：函数泛参/约束排在返回类型之前（`FuncDecl genFirst ⇒ GenericParam T ⇒ GenericConstraint ⇒ RefType Base ⇒ 返回类型 ⇒ 形参`）
        Traverse(*node.generic, visitor);
    }
    if (node.retType != nullptr && !node.retType->TestAnyAttr(Attribute::COMPILER_ADD)) {
        Traverse(*node.retType, visitor);
    }
    for (auto& pl : node.paramLists) {
        if (pl != nullptr) {
            Traverse(*pl, visitor);
        }
    }
    if (node.body != nullptr) {
        Traverse(*node.body, visitor);
    }
    parentIsFile_ = saved;
    res = VisitResult::Cont();
}

void AstTreePrintPass::Visit(const LambdaExpr& node, VisitResult& res)
{
    bool saved = parentIsFile_;
    parentIsFile_ = false;
    for (auto& c : AstNodeHelper::GetChildren(node)) {
        if (c->astKind == AstKind::BLOCK) {
            for (auto& cc : AstNodeHelper::GetChildren(*c)) {
                Traverse(*cc, visitor);
            }
        } else if (c->astKind == AstKind::FUNC_BODY) {
            // ★ golden：lambda 体的 Block 扁平（不出 `Block` 行），形参列表照出
            //   （实证：`{ a => a + 1 }` ⇒ LambdaExpr ⇒ FuncParam a ⇒ BinaryExpr +）
            auto& fb = Cast<const FuncBody&>(*c);
            for (auto& pl : fb.paramLists) {
                if (pl != nullptr) {
                    Traverse(*pl, visitor);
                }
            }
            if (fb.body != nullptr) {
                for (auto& cc : AstNodeHelper::GetChildren(*fb.body)) {
                    Traverse(*cc, visitor);
                }
            }
        } else {
            Traverse(*c, visitor);
        }
    }
    parentIsFile_ = saved;
    res = VisitResult::Cont();
}

void AstTreePrintPass::Visit(const TypeConvExpr& node, VisitResult& res)
{
    bool saved = parentIsFile_;
    parentIsFile_ = false;
    if (node.expr != nullptr) {
        Traverse(*node.expr, visitor);
    }
    if (node.type != nullptr) {
        Traverse(*node.type, visitor);
    }
    parentIsFile_ = saved;
    res = VisitResult::Cont();
}

void AstTreePrintPass::TraverseTypeTestChildren(const AstNode& node, VisitResult& res)
{
    // ★ 语义正确的口径：IsExpr/AsExpr = 表达式 + 类型，两面都要出（类型被丢是**基线**的缺陷，cjah 不跟）
    //   现状基线只出表达式面 ⇒ cjah 此处会多 2 行（两个 IsExpr/AsExpr 的类型行），待基线修正后归零。
    bool saved = parentIsFile_;
    parentIsFile_ = false;
    for (auto& c : AstNodeHelper::GetChildren(node)) {
        if (c != nullptr) {
            Traverse(*c, visitor);
        }
    }
    parentIsFile_ = saved;
    res = VisitResult::Cont();
}

void AstTreePrintPass::Visit(const ArrayExpr& node, VisitResult& res)
{
    if (node.isValueArray) {
        bool saved = parentIsFile_;
        parentIsFile_ = false;
        for (auto& arg : node.args) {
            if (arg != nullptr) {
                Traverse(*arg, visitor);
            }
        }
        parentIsFile_ = saved;
    } else {
        this->WalkChildren(node);
    }
    res = VisitResult::Cont();
}

void AstTreePrintPass::Visit(const VarWithPatternDecl& node, VisitResult& res)
{
    bool saved = parentIsFile_;
    parentIsFile_ = false;
    if (node.irrefutablePattern != nullptr && node.irrefutablePattern->astKind == AstKind::WILDCARD_PATTERN) {
        Traverse(*node.irrefutablePattern, visitor);
        // 通配模式（`var _ = e`）不出模式行，直接出初始化式
        if (node.initializer != nullptr) {
            Traverse(*node.initializer, visitor);
        }
    } else {
        // ★ 模式子树**照常下钻**（与 ForInExpr / LetPatternDestructor 同口径：模式是语义绑定位）。
        //   源码序 `<pattern> [: <type>] = <initializer>`。
        //   ⚠️ 初始化式必须在此显式下探：`VAR_WITH_PATTERN_DECL` 的收集器只纳入
        //      `irrefutablePattern`（走 `CollectChildren(const Decl&)` ⇒ 不含 type/initializer），
        //      靠 WalkChildren 会整段丢掉 `let (a, b) = (1, 2)` 的右值 ✗。
        if (node.irrefutablePattern != nullptr) {
            Traverse(*node.irrefutablePattern, visitor);
        }
        if (node.type != nullptr) {
            Traverse(*node.type, visitor);
        }
        if (node.initializer != nullptr) {
            Traverse(*node.initializer, visitor);
        }
    }
    parentIsFile_ = saved;
    res = VisitResult::Cont();
}

void AstTreePrintPass::Visit(const VarPattern& node, VisitResult& res)
{
    bool saved = parentIsFile_;
    parentIsFile_ = false;
    if (node.varDecl != nullptr) {
        for (auto& c : AstNodeHelper::GetChildren(*node.varDecl)) {
            Traverse(*c, visitor);
        }
    }
    parentIsFile_ = saved;
    res = VisitResult::Cont();
}

void AstTreePrintPass::TraversePatternExprChildren(const AstNode& node, VisitResult& res)
{
    bool saved = parentIsFile_;
    parentIsFile_ = false;
    for (auto& c : AstNodeHelper::GetChildren(node)) {
        if (c == nullptr) {
            continue;
        }
        // 模式子级**照常出行**：`let <pattern> <- e` 的模式（`Some(x)` / `VarPattern` …）
        // 是语义绑定位 ⇒ 不打印会让参照侧「看着没有」这些绑定位（基线改用例见
        // `duration.cj:123` 一族的 `EnumPattern`/`VarOrEnumPattern`）
        Traverse(*c, visitor);
    }
    parentIsFile_ = saved;
    res = VisitResult::Cont();
}

void AstTreePrintPass::Visit(const ImportSpec& node, VisitResult& res)
{
    bool savedTop = parentIsFile_;
    parentIsFile_ = false;
    inNestedImport_ = true;
    for (auto& child : AstNodeHelper::GetChildren(node)) {
        Traverse(*child, visitor);
    }
    inNestedImport_ = false;
    Visit(node.content, res);   // content 自补须在恢复前（老口径：此处 topLevel=false）
    parentIsFile_ = savedTop;
    res = VisitResult::Cont();
}

void AstTreePrintPass::Visit(const EnumDecl& node, VisitResult& res)
{
    bool savedTop = parentIsFile_;
    parentIsFile_ = false;
    inEnumBody_ = true;
    for (auto& child : AstNodeHelper::GetChildren(node)) {
        Traverse(*child, visitor);
    }
    inEnumBody_ = false;
    parentIsFile_ = savedTop;
    res = VisitResult::Cont();
}

void AstTreePrintPass::TraverseTypeAnnotationLast(const AstNode& node, VisitResult& res)
{
    this->WalkChildren(node);
    this->AppendTypeAnnotation(node);
    res = VisitResult::Cont();
}

void AstTreePrintPass::Visit(const PropDecl& node, VisitResult& res)
{
    this->AppendTypeAnnotation(node);
    this->WalkChildren(node);
    res = VisitResult::Cont();
}

void AstTreePrintPass::Visit(const VArrayType& node, VisitResult& res)
{
    for (auto& child : AstNodeHelper::GetChildren(node)) {
        if (child != nullptr && child->astKind != AstKind::CONSTANT_TYPE) {
            Traverse(*child, visitor);
        }
    }
    res = VisitResult::Cont();
}

void AstTreePrintPass::Visit(const MemberAccess& node, VisitResult& res)
{
    this->WalkChildren(node);
    this->AppendMemberTypeArgs(node);
    res = VisitResult::Cont();
}

void AstTreePrintPass::Run(AstNode& node)
{
    CreateDirIfNotExists(Config().out);
    const Package* pkg = nullptr;
    if (node.astKind == AstKind::PACKAGE) {
        pkg = &Cast<const Package&>(node);
    } else {
        for (auto& child : AstNodeHelper::GetChildren(node)) {
            if (child != nullptr && child->astKind == AstKind::PACKAGE) {
                pkg = &Cast<const Package&>(*child);
                break;
            }
        }
    }
    if (pkg == nullptr) {
        return;
    }
    const ConStr& treePath = Config().out.empty() ? (pkg->fullPackageName + ".tree")
                                                  : (Config().out + "/" + pkg->fullPackageName + ".tree");
    ofs.open(treePath, std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) {
        LOGE("open file failed: ", treePath);
        return;
    }
    // 一包一文件：多源文件按序拼接、文件间不留空行。
    // ★ 顺序：**按文件名升序**（基名相同则用完整路径兜底）。
    //   既不按命令行参数序，也不按 `pkg->files` 的 AST 序 —— 后者取决于前端
    //   目录遍历/glob 次序，不保证跨机器稳定 ⇒ 同一包多文件的 .tree 不可复现。
    std::vector<const File*> ordered;
    for (auto& file : pkg->files) {
        if (file != nullptr) {
            ordered.push_back(file.get());
        }
    }
    std::stable_sort(ordered.begin(), ordered.end(), [](const File* lhs, const File* rhs) {
        ConStr lName = FileName(lhs->fileName);
        ConStr rName = FileName(rhs->fileName);
        if (lName != rName) {
            return lName < rName;
        }
        return lhs->fileName < rhs->fileName;   // 基名相同（如 a.cj / a.cj.bak）时保证全序
    });
    for (auto* file : ordered) {
        (void)Traverse(*file, visitor);
    }
    PrintBuiltinDecls(*pkg);
    PRT().Flush();
}

/**
 * @brief 包级内建声明（`BuiltInDecl`）统一收在**所有源码文件块之后**打印（用户口径 2026-10-27）。
 *
 * 由来：前端 sema 阶段给 `std.core` 注入 `RawArray` / `VArray` / `CPointer` / `CFunc` / `CString`
 * （`Sema/TypeChecker.cpp` 的 `AddBuiltIn*Decl`），注入点**硬编码**为 `pkg->files[0]->decls`
 * 末尾；而前端又按 `fileName` 字典序排序 `pkg->files`（`Frontend/CompileStrategy.cpp`）
 * ⇒ 换个输入文件集就"搬家"（实测：`annotation.cj` 恰好是最小的文件名，于是这批节点看起来
 * 像 annotation.cj 的源码声明 ✗）。
 *
 * 处理：源码文件块遍历时跳过它们（见 WalkFileChildren），全部集中到末尾一个 `<builtin>`
 * 伪文件块里打印；顺序保持前端注入序（不做排序，便于与前端行为对照）。
 */
void AstTreePrintPass::PrintBuiltinDecls(const Package& pkg)
{
    std::vector<const AstNode*> builtins;
    for (auto& file : pkg.files) {
        if (file == nullptr) {
            continue;
        }
        for (auto& decl : file->decls) {
            if (decl != nullptr && decl->astKind == AstKind::BUILTIN_DECL) {
                builtins.emplace_back(decl.get());
            }
        }
    }
    if (builtins.empty()) {
        return;   // 非 std.core（或其他未注入的包）不出现该块
    }
    // 与源码文件根行同形：`File // <名> `（名后一空格、不定位）
    PRT().PVal("File").PVal(" // <builtin> ").PNL();
    PRT().Indent();
    bool saved = parentIsFile_;
    parentIsFile_ = false;
    for (auto* decl : builtins) {
        Traverse(*decl, visitor);
    }
    parentIsFile_ = saved;
    PRT().Unindent();
}

/** @brief 自注册（本库被 dlopen 时生效；名称须在 config/passes.json 的 names 中登记）。
 *
 *  ★缺此注册的后果：框架 TryGetPass 中 LoadPass 成功后重试仍无 builder ⇒ 反复 dlopen ⇒ 栈溢出段错误。
 */
static PassManager::Register g_astTreePrintReg("ast-tree-print", [](const PassConfig& cfg) -> UniquePtr<Pass> {
    return std::make_unique<AstTreePrintPass>(Cast<const ToSourcePassConfig&>(cfg));
});
