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
            return "func#(" + FmtList(params) + ")->" + ret + "#tp:[]#opt:0#var:"
                + (ft.hasVariableLenArg ? "true" : "false") + "#names:[]";
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
            // 上界：Nominal 的 tp 段按 typechecker 约定输出名字[上界] 形式；
            // 编译器泛型上界在 GenericsTy.upperBounds，声明级此处取 generic 约束不可达 → 输出空上界
            StrVec tp;
            return kindStr + "#" + decl->fullPackageName + "#" + decl->identifier.Val() + "#ta:" + FmtList(ta)
                + "#sup:[]#tp:" + FmtList(tp);
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
        prt.PVals("file: ", i, " ", pkg.files[i]->fileName).PNL();
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
    }
}

void DumpSemanticResultPass::CollectDeclSymbol(const Decl& decl, int fileIdx)
{
    const Str kind = SymKindOf(decl);
    if (!kind.empty()) {
        SymRow row;
        row.id = static_cast<int>(symRows.size());
        row.fileIdx = fileIdx;
        row.kind = kind;
        row.name = decl.identifier.Val();
        row.tyId = pool.Ensure(decl.GetTy());
        symRows.push_back(row);
    }
    // 成员声明递归（class/interface/struct/enum/extend body）
    for (auto& member : decl.GetMemberDecls()) {
        CollectDeclSymbol(*member, fileIdx);
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
        prt.PNL();
    }
}

void DumpSemanticResultPass::WriteBindings()
{
    prt.PValNL("bindings:");
    for (auto& [fname, rows] : bindFiles) {
        prt.PVals("  ", fname, ":").PNL();
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
                if (auto* decl = dynamic_cast<const Decl*>(&node)) {
                    auto name = decl->identifier.Val();
                    if (!name.empty()) {
                        identity += ":" + name;
                    }
                }
                rowsRef.push_back(BindRow{node.begin.line, node.begin.column, identity, poolRef.Ensure(ty)});
            }
        }
    }

private:
    SemanticTyPool& poolRef;
    Vec<BindRow>& rowsRef;
};

void DumpSemanticResultPass::CollectBindings(const Package& pkg)
{
    for (Size fi = 0; fi < pkg.files.size(); fi++) {
        auto& file = *pkg.files[fi];
        // 每文件独立 collector → 独立绑定段；行序 = begin(line,col) 排序
        bindFiles.emplace_back(file.fileName, Vec<BindRow>{});
        auto& rows = bindFiles.back().second;
        BindCollector collector(pool, rows);
        for (auto& decl : file.decls) {
            Traverse(*decl, collector);
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
