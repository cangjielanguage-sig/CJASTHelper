/**
 * @file
 *
 * This file implements the DumpSemanticResultPass.
 *
 * R120 文本格式 v1（对齐 typechecker serde result_ser.cj 行约定）：
 *
 *   # SemanticResult v1
 *   package <pkgName>
 *   files: <N>
 *   file: <idx> <fileKey>
 *   types:
 *   T0: Int64
 *   T1: class#pkg#Name#ta:[T2]#sup:[T0]#tp:[E]
 *   T2: func#([T1])->T3#tp:[]#opt:0#var:false#names:[]
 *   sym:
 *   S0@0: class#Name#ty:T1
 *   bindings:
 *     file0.cj:
 *       12:5:RefExpr -> T3
 */

#include "DumpSemanticResultPass.h"
#include "cangjie/AST/Types.h"
#include "utils/Logger.h"

REG_PASS("dump-semantic-result",
    ([](const PassConfig& config) { return UniquePtr<Pass>(new DumpSemanticResultPass{Cast<const ToSourcePassConfig&>(config)}); }));

namespace {
/**
 * @brief TypeKind → R120 稳定名（PrimitiveTy 族按 TypeKind 输出 enum 名）
 */
Str Kind2Str(Cangjie::AST::TypeKind kind)
{
    using CK = Cangjie::AST::TypeKind;
    switch (kind) {
        case CK::TYPE_UNIT:
            return "Unit";
        case CK::TYPE_INT8:
            return "Int8";
        case CK::TYPE_INT16:
            return "Int16";
        case CK::TYPE_INT32:
            return "Int32";
        case CK::TYPE_INT64:
            return "Int64";
        case CK::TYPE_INT_NATIVE:
            return "IntNative";
        case CK::TYPE_IDEAL_INT:
            return "Int";
        case CK::TYPE_UINT8:
            return "UInt8";
        case CK::TYPE_UINT16:
            return "UInt16";
        case CK::TYPE_UINT32:
            return "UInt32";
        case CK::TYPE_UINT64:
            return "UInt64";
        case CK::TYPE_UINT_NATIVE:
            return "UIntNative";
        case CK::TYPE_FLOAT16:
            return "Float16";
        case CK::TYPE_FLOAT32:
            return "Float32";
        case CK::TYPE_FLOAT64:
            return "Float64";
        case CK::TYPE_IDEAL_FLOAT:
            return "Float";
        case CK::TYPE_RUNE:
            return "Rune";
        case CK::TYPE_BOOLEAN:
            return "Bool";
        case CK::TYPE_CSTRING:
            return "CString";
        case CK::TYPE_NOTHING:
            return "Nothing";
        default:
            return "";
    }
}

/**
 * @brief AstKind → R120 符号 kind 名
 */
Str SymKind2Str(AstKind kind)
{
    switch (kind) {
        case AstKind::CLASS_DECL:
            return "class";
        case AstKind::INTERFACE_DECL:
            return "interface";
        case AstKind::STRUCT_DECL:
            return "struct";
        case AstKind::ENUM_DECL:
            return "enum";
        case AstKind::FUNC_DECL:
            return "func";
        case AstKind::VAR_DECL:
            return "var";
        case AstKind::PROP_DECL:
            return "prop";
        case AstKind::TYPE_ALIAS_DECL:
            return "alias";
        case AstKind::EXTEND_DECL:
            return "extend";
        default:
            return "";
    }
}
} // namespace

// === SemanticTyPool ===

bool SemanticTyPool::IsNominalKind(Cangjie::AST::TypeKind kind)
{
    return kind == Cangjie::AST::TypeKind::TYPE_CLASS || kind == Cangjie::AST::TypeKind::TYPE_INTERFACE
        || kind == Cangjie::AST::TypeKind::TYPE_STRUCT || kind == Cangjie::AST::TypeKind::TYPE_ENUM;
}

Str SemanticTyPool::NominalTextKey(const Cangjie::AST::Ty& ty)
{
    // kind#pkg#name（typeArgs 经递归 Ensure 不进 key——首层靠 id 序可再合并；
    // 跨实例同名类不同实参场景极少且此处 golden 只需可复现一致性）
    Str pkgName;
    Str name;
    Ptr<const Cangjie::AST::Decl> decl = nullptr;
    using namespace Cangjie::AST;
    if (auto* ct = dynamic_cast<const ClassTy*>(&ty)) {
        decl = ct->decl;
    } else if (auto* it = dynamic_cast<const InterfaceTy*>(&ty)) {
        decl = it->decl;
    } else if (auto* st = dynamic_cast<const StructTy*>(&ty)) {
        decl = st->decl;
    } else if (auto* et = dynamic_cast<const EnumTy*>(&ty)) {
        decl = et->decl;
    }
    if (decl) {
        pkgName = decl->fullPackageName;
        name = decl->identifier.Val();
    }
    return Str(ty.name) + "#" + pkgName + "#" + name;
}

int SemanticTyPool::Ensure(const Cangjie::AST::Ty* ty)
{
    if (!ty) {
        return -1;
    }
    return DoEnsure(ty);
}

int SemanticTyPool::EnsureWithMeta(const Cangjie::AST::Ty* ty, const FuncDeclMeta* meta)
{
    if (!ty) {
        return -1;
    }
    const int id = DoEnsure(ty);
    if (meta && funcMetas.find(ty) == funcMetas.end()) {
        funcMetas.emplace(ty, *meta);
    }
    return id;
}

int SemanticTyPool::DoEnsure(const Cangjie::AST::Ty* ty)
{
    const bool nominal = IsNominalKind(ty->kind);
    TyKey key{ty, !nominal, nominal ? NominalTextKey(*ty) : Str("")};
    if (auto it = t2id.find(key); it != t2id.end()) {
        return it->second;
    }
    // 仅登记 id（编码延迟到 ResolvedLines 按 id 序统一执行，保证行顺序 = id 顺序）
    const int id = static_cast<int>(order.size());
    t2id.emplace(key, id);
    order.push_back(ty);
    return id;
}

/**
 * @brief 逗号列表格式化（`[a,b]`；R120 列表字段共用）
 */
static Str FmtList(const StrVec& items)
{
    if (items.empty()) {
        return "[]";
    }
    Str out = "[";
    for (Size i = 0; i < items.size(); i++) {
        if (i) {
            out += ",";
        }
        out += items[i];
    }
    out += "]";
    return out;
}

Str SemanticTyPool::FmtList(const StrVec& items)
{
    if (items.empty()) {
        return "[]";
    }
    Str out = "[";
    for (Size i = 0; i < items.size(); i++) {
        if (i) {
            out += ",";
        }
        out += items[i];
    }
    return out + "]";
}

Str SemanticTyPool::Encode(const Cangjie::AST::Ty* ty)
{
    using namespace Cangjie::AST;
    const auto ensure = [this](const Ty* t) { return "T" + std::to_string(this->Ensure(t)); };

    switch (ty->kind) {
        case TypeKind::TYPE_TUPLE: {
            StrVec args;
            for (auto& a : ty->typeArgs) {
                args.push_back(ensure(a));
            }
            return "tuple#" + FmtList(args);
        }
        case TypeKind::TYPE_FUNC: {
            auto& ft = *static_cast<const FuncTy*>(ty);
            StrVec params;
            // FuncTy 构造后 typeArgs = paramTys + [retTy]
            for (Size i = 0; i + 1 < ft.typeArgs.size(); i++) {
                params.push_back(ensure(ft.typeArgs[i]));
            }
            auto ret = ensure(ft.retTy);
            // CJAH-5c (G-3): names/opt 从 decl 上下文补齐（FuncTy 本体无此信息）——
            // names 形状 [[n1,n2]] 对齐 TC fmtList(ArrayList.toString) 双层括号；opt = 默认参数个数
            StrVec names;
            int optCount = 0;
            if (auto it = funcMetas.find(ty); it != funcMetas.end()) {
                for (auto& n : it->second.paramNames) {
                    names.push_back(n);
                }
                optCount = it->second.optionalParamCount;
            }
            // names 形状对齐 TC：无参 = names:[]（fmtList(空)）；有参 = names:[[a,b]]（fmtList(ArrayList.toString) 双层）
            const Str namesSeg = names.empty() ? Str("names:[]") : Str("names:[" + FmtList(names) + "]");
            return "func#(" + FmtList(params) + ")->" + ret + "#tp:[]#opt:" + std::to_string(optCount)
                + "#var:" + (ft.hasVariableLenArg ? "true" : "false") + "#" + namesSeg;
        }
        case TypeKind::TYPE_ARRAY: {
            auto& at = *static_cast<const ArrayTy*>(ty);
            return "rawarray#" + ensure(at.typeArgs[0]) + "#" + std::to_string(at.dims);
        }
        case TypeKind::TYPE_VARRAY: {
            auto& vt = *static_cast<const VArrayTy*>(ty);
            return "varray#" + ensure(vt.typeArgs[0]) + "#" + std::to_string(vt.size);
        }
        case TypeKind::TYPE_POINTER:
            return "cpointer#" + ensure(ty->typeArgs[0]);
        case TypeKind::TYPE_CLASS:
        case TypeKind::TYPE_INTERFACE:
        case TypeKind::TYPE_STRUCT:
        case TypeKind::TYPE_ENUM: {
            const Str kindStr = [&] {
                switch (ty->kind) {
                    case TypeKind::TYPE_CLASS:
                        return "class";
                    case TypeKind::TYPE_INTERFACE:
                        return "interface";
                    case TypeKind::TYPE_STRUCT:
                        return "struct";
                    case TypeKind::TYPE_ENUM:
                        return "enum";
                    default:
                        return "nominal?";
                }
            }();
            Ptr<const Decl> decl = nullptr;
            if (auto* ct = dynamic_cast<const ClassTy*>(ty)) {
                decl = ct->decl;
            } else if (auto* it = dynamic_cast<const InterfaceTy*>(ty)) {
                decl = it->decl;
            } else if (auto* st = dynamic_cast<const StructTy*>(ty)) {
                decl = st->decl;
            } else if (auto* et = dynamic_cast<const EnumTy*>(ty)) {
                decl = et->decl;
            }
            if (!decl) {
                return kindStr + "#?#name?";
            }
            StrVec ta;
            for (auto& a : ty->typeArgs) {
                ta.push_back(ensure(a));
            }
            // CJAH-5a (G-1): sup 段收声明位直接父类型（对齐 TC 口径 = AST Decl.superTypes 源码级列表）。
            // 口径注记：编译器 Sema 给无显式父类的 class 注入隐式 Object 节点（inheritedTypes 实证），
            // CJAH sup 会含该 Object 行（TC 侧无——std.ast parse 无编译器补节点）；恢复侧 Object 可闭合，
            // 差异属「CJAH 信息超集」，对拍按显式声明位子集归一。全链接口展开（Comparable→Equatable…）
            // 是 std.core 源码声明位本就多继承，非 CJAH 展开。
            StrVec sup;
            const Decl* declRaw = decl;
            if (auto* inh = dynamic_cast<const InheritableDecl*>(declRaw)) {
                for (auto& st : inh->inheritedTypes) {
                    if (st && st->GetTy() && st->GetTy()->kind != Cangjie::AST::TypeKind::TYPE_INVALID
                        && st->GetTy()->kind != Cangjie::AST::TypeKind::TYPE_INITIAL) {
                        sup.push_back(ensure(st->GetTy()));
                    }
                }
            }
            // 上界：Nominal 的 tp 段按 typechecker 约定输出名字[上界] 形式；
            // 编译器泛型上界在 GenericsTy.upperBounds，声明级此处取 generic 约束不可达 → 输出空上界
            StrVec tp;
            return kindStr + "#" + decl->fullPackageName + "#" + decl->identifier.Val() + "#ta:" + FmtList(ta)
                + "#sup:" + FmtList(sup) + "#tp:" + FmtList(tp);
        }
        case TypeKind::TYPE_GENERICS: {
            auto& gt = *static_cast<const GenericsTy*>(ty);
            StrVec ubs;
            for (auto& ub : gt.upperBounds) {
                ubs.push_back(ensure(ub));
            }
            return "generic#" + gt.name + "#" + FmtList(ubs);
        }
        default:
            // 单例族：Unit/Int8..Float64/Rune/Bool/CString/Nothing 等
            return Kind2Str(ty->kind);
    }
}

// === DumpSemanticResultPass ===

void DumpSemanticResultPass::Run(AstNode& node)
{
    if (node.astKind != AstKind::PACKAGE) {
        LOGE("DumpSemanticResultPass expects Package node, got: ", AstKind2Str(node.astKind));
        return;
    }
    auto& pkg = Cast<Package&>(node);
    auto outDir = Config().out.empty() ? Str(".") : Config().out;
    CreateDirIfNotExists(outDir);
    auto path = outDir + "/" + pkg.fullPackageName + ".semantic-result";
    ofs.open(path, std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) {
        LOGE("open file failed: ", path);
        return;
    }
    LOGI("Dump semantic result to: ", path);

    // 绑定口径配置：CJAH_SER_BIND_SCOPE=expr → 仅表达式节点（对齐 typechecker typeBindings）
    if (const char* scopeEnv = std::getenv("CJAH_SER_BIND_SCOPE"); scopeEnv && Str(scopeEnv) == "expr") {
        bindScope = BindScope::EXPR;
        LOGI("bind scope: expr-only");
    }
    // CJAH-4d: 文件段名口径——CJAH_SER_FILE_KEY=virtual → 输出 TC 虚拟名 `file<i>.cj <pkg>`
    // （默认 real：保留真实文件名；对拍按 file 序位置对齐两种口径）
    virtualFileKey = false;
    if (const char* keyEnv = std::getenv("CJAH_SER_FILE_KEY"); keyEnv && Str(keyEnv) == "virtual") {
        virtualFileKey = true;
        LOGI("file key: virtual (file<i>.cj)");
    }

    DumpPackage(pkg);
    ofs.flush();
    ofs.close();
}

void DumpSemanticResultPass::DumpPackage(const Package& pkg)
{
    // 两遍写：先收集（符号收集触发池登记），再按 Header → types → sym → bindings 写出
    CollectSymbols(pkg);
    CollectBindings(pkg);

    WriteHeader(pkg);
    WriteTypes();
    WriteSymbols();
    WriteBindings();
}

void DumpSemanticResultPass::CollectSymbols(const Package& pkg)
{
    for (Size fi = 0; fi < pkg.files.size(); fi++) {
        CollectFileSymbols(*pkg.files[fi], static_cast<int>(fi));
    }
}

void DumpSemanticResultPass::WriteHeader(const Package& pkg)
{
    prt.PValNL("# SemanticResult v1");
    prt.PVals("package ", pkg.fullPackageName).PNL();
    prt.PVals("files: ", pkg.files.size()).PNL();
    for (Size i = 0; i < pkg.files.size(); i++) {
        if (virtualFileKey) {
            // CJAH-4d: TC 虚拟名口径 `file<i>.cj <pkg>`（对拍按位置对齐）
            prt.PVals("file: ", i, " file", i, ".cj ", pkg.fullPackageName).PNL();
        } else {
            prt.PVals("file: ", i, " ", pkg.files[i]->fileName).PNL();
        }
    }
}

void DumpSemanticResultPass::WriteTypes()
{
    prt.PValNL("types:");
    for (auto& line : pool.ResolvedLines()) {
        prt.PValNL(line);
    }
}

void DumpSemanticResultPass::CollectFileSymbols(const File& file, int fileIdx)
{
    for (auto& decl : file.decls) {
        CollectDeclSymbol(*decl, fileIdx);
        // CJAH-4c: main() 函数体局部声明（VarDecl）→ symbol 行（对齐 TC LocalSymbol）
        CollectBodySymbols(*decl, fileIdx);
    }
}

/**
 * @brief 函数体局部声明收集（CJAH-4c）：递归 FuncBody.block 一级 VarDecl/VarPattern
 *        （对齐 TC LocalSymbol 收集面——TC 收函数体直系 let/var，不递归嵌套 lambda 内部）
 *        main() 经 B120RC3 desugar → desugarDecl(FuncDecl) 优先取
 */
void DumpSemanticResultPass::CollectBodySymbols(const Decl& decl, int fileIdx)
{
    const FuncBody* body = nullptr;
    if (auto* main = dynamic_cast<const MainDecl*>(&decl)) {
        body = main->desugarDecl && main->desugarDecl->funcBody ? main->desugarDecl->funcBody.get()
                                                                : main->funcBody.get();
    } else if (auto* func = dynamic_cast<const FuncDecl*>(&decl)) {
        body = func->funcBody.get();
    }
    if (!body || !body->body) {
        return;
    }
    for (auto& stmt : body->body->body) {
        if (!stmt) {
            continue;
        }
        if (stmt->astKind != AstKind::VAR_DECL) {
            continue;
        }
        if (auto* var = Cast<VarDecl*>(stmt.get())) {
            SymRow row;
            row.id = static_cast<int>(symRows.size());
            row.fileIdx = fileIdx;
            row.kind = "var";
            row.name = var->identifier.Val();
            row.tyId = pool.Ensure(var->GetTy());
            symRows.push_back(row);
        }
    }
}

void DumpSemanticResultPass::CollectDeclSymbol(const Decl& decl, int fileIdx, bool inExtend)
{
    const Str kind = SymKindOf(decl);
    if (!kind.empty()) {
        // CJAH-5c: func 声明先经 EnsureWithMeta 登记上下文（参数名/默认参数数）——
        // 绑定/符号收集引用同结构 FuncTy 时 Encode 可取 names/opt
        UniquePtr<FuncDeclMeta> metaHolder;
        const FuncDeclMeta* metaPtr = nullptr;
        if (auto* func = dynamic_cast<const FuncDecl*>(&decl); func && func->funcBody) {
            metaHolder.reset(new FuncDeclMeta());
            for (auto& paramList : func->funcBody->paramLists) {
                for (auto& param : paramList->params) {
                    metaHolder->paramNames.push_back(param->identifier.Val());
                    if (param->assignment) {
                        metaHolder->optionalParamCount++;
                    }
                }
            }
            metaPtr = metaHolder.get();
            (void)pool.EnsureWithMeta(decl.GetTy(), metaPtr);
        }
        SymRow row;
        row.id = static_cast<int>(symRows.size());
        row.fileIdx = fileIdx;
        // CJAH-5b (G-2): extend body 成员用 emember 记号（对齐 TC restore 词表——
        // result_restore.cj:116 emember 分支挂 curExtend 桶；extend 行本身 = extend#<被扩展类型名>）
        row.kind = inExtend ? "emember" : kind;
        if (auto* extend = dynamic_cast<const ExtendDecl*>(&decl)) {
            // extend 行名 = 被扩展类型名（extendedType Sema 后取 Ty 名；Nominal 优先 decl identifier）
            Str extName;
            const Cangjie::AST::Ty* ty = extend->extendedType ? extend->extendedType->GetTy() : nullptr;
            if (ty) {
                extName = ty->name;
                if (extName.empty()) {
                    using namespace Cangjie::AST;
                    if (auto* nt = dynamic_cast<const ClassLikeTy*>(ty)) {
                        extName = nt->commonDecl ? nt->commonDecl->identifier.Val() : Str("");
                    }
                }
            }
            row.name = extName.empty() ? decl.identifier.Val() : extName;
        } else {
            row.name = decl.identifier.Val();
        }
        row.tyId = pool.Ensure(decl.GetTy());
        // CJAH-4b: func/member 声明携带参数类型表（params:[[T…]]，对齐 TC 签名可比格式）
        if (auto* func = dynamic_cast<const FuncDecl*>(&decl); func && func->funcBody) {
            for (auto& paramList : func->funcBody->paramLists) {
                for (auto& param : paramList->params) {
                    auto tid = pool.Ensure(param->GetTy());
                    row.paramTyIds.push_back("T" + std::to_string(tid));
                }
            }
        }
        symRows.push_back(row);
    }
    // 成员声明递归（class/interface/struct/enum/extend body）；extend body 内标记 inExtend
    const bool membersInExtend = inExtend || decl.astKind == AstKind::EXTEND_DECL;
    for (auto& member : decl.GetMemberDecls()) {
        CollectDeclSymbol(*member, fileIdx, membersInExtend);
    }
}

void DumpSemanticResultPass::WriteSymbols()
{
    prt.PValNL("sym:");
    for (auto& row : symRows) {
        prt.PVals("S", row.id, "@", row.fileIdx, ": ", row.kind, "#", row.name, "#ty:");
        if (row.tyId >= 0) {
            prt.PVals("T", row.tyId);
        } else {
            prt.PVal("-");
        }
        // CJAH-4b: 参数类型表 `#params:[[T…]]`（对齐 TC 符号行签名可比字段）
        if (!row.paramTyIds.empty()) {
            prt.PVal("#params:[[" + FmtList(row.paramTyIds) + "]]");
        }
        prt.PNL();
    }
}

void DumpSemanticResultPass::WriteBindings()
{
    prt.PValNL("bindings:");
    for (auto& [fname, rows] : bindFiles) {
        Str key = fname;
        if (virtualFileKey) {
            // CJAH-4d: bindings 文件键与 file 段口径一致（TC 虚拟名 file<i>.cj）
            for (Size i = 0; i < bindFiles.size(); i++) {
                if (bindFiles[i].first == fname) {
                    key = "file" + std::to_string(i) + ".cj";
                    break;
                }
            }
        }
        prt.PVals("  ", key, ":").PNL();
        for (auto& r : rows) {
            prt.PVals("    ", r.line, ":", r.col, ":", r.identity, " -> T", r.tyId).PNL();
        }
    }
}

/**
 * @brief 绑定收集 visitor：全量遍历，为每个有语义类型的节点生成绑定行
 */
class BindCollector : public ConstAstVisitor {
public:
    BindCollector(SemanticTyPool& poolRef, Vec<BindRow>& rowsRef) : poolRef(poolRef), rowsRef(rowsRef)
    {
    }

protected:
    void AfterVisit(const AstNode& node, const VisitResult& res) override
    {
        // 过滤编译器合成节点（begin 未解析：line==0）与无语义类型节点
        if (node.begin.line == 0) {
            return;
        }
        if (auto ty = node.GetTy()) {
            // 跳过编译器初始占位类型（未过 Sema 节点）
            if (ty->kind != Cangjie::AST::TypeKind::TYPE_INITIAL && ty->kind != Cangjie::AST::TypeKind::TYPE_INVALID) {
                Str identity = AstKind2Str(node.astKind);
                // AstKind2Str 可能含空格（如 " func_body"），净化为 R120 安全 identity
                identity.erase(0, identity.find_first_not_of(" \t"));
                // identity 增补：Decl 或表达式类别点带语义 identifier（对齐 typechecker NodeKey =
                // line:col:kind:name；RefExpr→引用名, MemberAccess→成员名, CallExpr→被调名）
                Str name = NameSuffixOf(node);
                if (!name.empty()) {
                    identity += ":" + name;
                }
                // scope 过滤：EXPR 口径只收表达式节点（对齐 typechecker typeBindings 收集面）
                if (scope == BindScope::EXPR && !IsExprKind(node.astKind)) {
                    return;
                }
                rowsRef.push_back(BindRow{node.begin.line, node.begin.column, identity, poolRef.Ensure(ty)});
            }
        }
    }

    /**
     * @brief 节点语义名：Decl 取 identifier；RefExpr 取引用名；MemberAccess 取成员名；
     *        CallExpr 取被调可见名（调用目标 identifier）
     */
    static Str NameSuffixOf(const AstNode& node)
    {
        if (auto* decl = dynamic_cast<const Decl*>(&node)) {
            return decl->identifier.Val();
        }
        switch (node.astKind) {
            case AstKind::REF_EXPR: {
                auto* ref = dynamic_cast<const RefExpr*>(&node);
                if (ref && ref->ref.identifier.Val() != "") {
                    return ref->ref.identifier.Val();
                }
                break;
            }
            case AstKind::MEMBER_ACCESS: {
                auto* ma = dynamic_cast<const MemberAccess*>(&node);
                if (ma && ma->field.Val() != "") {
                    return ma->field.Val();
                }
                break;
            }
            case AstKind::CALL_EXPR: {
                auto* call = dynamic_cast<const CallExpr*>(&node);
                if (!call) {
                    break;
                }
                // 被调可见名：baseFunc 为 RefExpr → 引用名；MemberAccess → 成员名（对齐 TC target name）
                if (call->baseFunc) {
                    const Expr* base = call->baseFunc.get();
                    if (base->astKind == AstKind::REF_EXPR) {
                        if (auto* ref = dynamic_cast<const RefExpr*>(base); ref && ref->ref.identifier.Val() != "") {
                            return ref->ref.identifier.Val();
                        }
                    } else if (base->astKind == AstKind::MEMBER_ACCESS) {
                        if (auto* ma = dynamic_cast<const MemberAccess*>(base); ma && ma->field.Val() != "") {
                            return ma->field.Val();
                        }
                    }
                }
                // 兜底：Sema 后经 resolvedFunction 取已解析目标名
                if (call->resolvedFunction && call->resolvedFunction->identifier.Val() != "") {
                    return call->resolvedFunction->identifier.Val();
                }
                break;
            }
            default:
                break;
        }
        return Str("");
    }

    /**
     * @brief 表达式族节点判定（typechecker typeBindings 收集口径同名集）
     */
    static bool IsExprKind(AstKind kind)
    {
        switch (kind) {
            case AstKind::REF_EXPR:
            case AstKind::MEMBER_ACCESS:
            case AstKind::CALL_EXPR:
            case AstKind::BINARY_EXPR:
            case AstKind::UNARY_EXPR:
            case AstKind::ASSIGN_EXPR:
            case AstKind::LIT_CONST_EXPR:
            case AstKind::RETURN_EXPR:
            case AstKind::SUBSCRIPT_EXPR:
            case AstKind::TUPLE_LIT:
            case AstKind::ARRAY_LIT:
            case AstKind::ARRAY_EXPR:
            case AstKind::LAMBDA_EXPR:
            case AstKind::RANGE_EXPR:
            case AstKind::TRAIL_CLOSURE_EXPR:
            case AstKind::AS_EXPR:
            case AstKind::IS_EXPR:
            case AstKind::OPTIONAL_CHAIN_EXPR:
            case AstKind::INC_OR_DEC_EXPR:
            case AstKind::STR_INTERPOLATION_EXPR:
            case AstKind::INTERPOLATION_EXPR:
            case AstKind::TYPE_CONV_EXPR:
            case AstKind::JUMP_EXPR:
            case AstKind::THROW_EXPR:
            case AstKind::MATCH_EXPR:
            case AstKind::IF_EXPR:
            case AstKind::SPAWN_EXPR:
            case AstKind::SYNCHRONIZED_EXPR:
            case AstKind::PAREN_EXPR:
            case AstKind::OPTIONAL_EXPR:
            case AstKind::POINTER_EXPR:
            case AstKind::WILDCARD_EXPR:
                return true;
            default:
                return false;
        }
    }

private:
    SemanticTyPool& poolRef;
    Vec<BindRow>& rowsRef;
    BindScope scope{BindScope::ALL};

public:
    void SetScope(BindScope s)
    {
        scope = s;
    }
};

void DumpSemanticResultPass::CollectBindings(const Package& pkg)
{
    for (Size fi = 0; fi < pkg.files.size(); fi++) {
        auto& file = *pkg.files[fi];
        // 每文件独立 collector → 独立绑定段；行序 = begin(line,col) 排序
        bindFiles.emplace_back(file.fileName, Vec<BindRow>{});
        auto& rows = bindFiles.back().second;
        BindCollector collector(pool, rows);
        collector.SetScope(bindScope);
        for (auto& decl : file.decls) {
            (void)Traverse(*decl, collector);
        }
        // 确定性排序：line/col/identity 升序（HashMap/遍历序漂移免疫）
        std::sort(rows.begin(), rows.end(), [](const BindRow& a, const BindRow& b) {
            if (a.line != b.line) {
                return a.line < b.line;
            }
            if (a.col != b.col) {
                return a.col < b.col;
            }
            return a.identity < b.identity;
        });
    }
}

Str DumpSemanticResultPass::SymKindOf(const Decl& decl)
{
    return SymKind2Str(decl.astKind);
}

Str DumpSemanticResultPass::IdentityOf(const AstNode& node)
{
    Str id = AstKind2Str(node.astKind);
    if (auto* decl = dynamic_cast<const Decl*>(&node)) {
        auto name = decl->identifier.Val();
        if (!name.empty()) {
            id += ":" + name;
        }
    }
    return id;
}
