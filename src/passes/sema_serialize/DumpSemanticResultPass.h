/**
 * @file
 *
 * This file declares the DumpSemanticResultPass.
 *
 * R120 文本格式 v1 序列化：语义类型（Ty 类型池）+ 符号图（Symbol）+ 绑定表
 * （node.begin + astKind → T<id>），输出 typechecker serde 兼容 golden 数据。
 */
#pragma once

#include "core/pass/Pass.h"
#include "cangjie/AST/Node.h"
#include "core/visitor/ConstAstVisitor.h"
#include "utils/Cast.h"
#include "utils/FileHelper.h"
#include "utils/Printer.h"
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <sstream>

/**
 * @class 语义类型池（编译器 Ty 指针 → T<id>，结构等价去重转发 Ty::Hash/operator==）
 */
class SemanticTyPool {
public:
    /**
     * @brief 确保类型已登记，返回 T-id（未登记则递归先登记子类型，再分配新 id）
     */
    int Ensure(const Cangjie::AST::Ty* ty);

    /**
     * @brief CJAH-5c: 带 decl 上下文的登记（func 类型行补 names:[..]/opt:N——
     *        编译器 FuncTy 无参数名/默认参数数字段，从 FuncDecl.funcBody.paramLists 补齐，
     *        R120 参照 §50 约定）。同 TyKey 已登记时复用首个 meta（TC 侧同结构共享 names，可接受）。
     */
    int EnsureWithMeta(const Cangjie::AST::Ty* ty, const class FuncDeclMeta* meta);

    /**
     * @brief 按 id 序统一编码类型行（首次调用时执行；子类型引用此时已被分配更大 id，
     *        保证行顺序 = id 顺序——R120 约定 reader 按行序即 id 序索引）
     */
    const StrVec& ResolvedLines()
    {
        if (resolved) {
            return lines;
        }
        resolved = true;
        // TC-12b：去重——按行文本检测重复（编译器 Ty 虚 Hash/== 对部分
        // 结构等价类型不收敛，文本 key 是唯一可靠判据），重复类型映射到首个 id，
        // 输出按新 id 顺序重排（跳过重复 → 行头 id 连续）。
        // 注意：Encode 内部调用 Ensure 可能注册新类型（order 动态增长），
        // 故循环条件用 order.size() 动态上界 + oldToNew 向量按需扩容。
        std::unordered_map<Str, int> textToFirstId;
        Vec<int> oldToNew;
        oldToNew.reserve(order.size());
        int seqId = 0;
        for (Size i = 0; i < order.size(); i++) {
            if (i >= oldToNew.size()) {
                oldToNew.resize(i + 1, -1);
            }
            Str encoded = Encode(order[i]);
            auto it = textToFirstId.find(encoded);
            if (it != textToFirstId.end()) {
                // 重复：映射到首个 id 的 new id（其 oldToNew 已定）
                oldToNew[i] = oldToNew[it->second];
                dupRemap_[static_cast<int>(i)] = oldToNew[i];
            } else {
                oldToNew[i] = seqId;
                lines.push_back("T" + std::to_string(seqId) + ": " + encoded);
                textToFirstId.emplace(std::move(encoded), static_cast<int>(i));
                seqId++;
            }
        }
        return lines;
    }

    Size Count() const
    {
        return t2id.size();
    }

    /** TC-12b: old_id → new_output_id（去重后映射；未去重时返回原 id） */
    int RemapId(int oldId) const
    {
        auto it = dupRemap_.find(oldId);
        return it != dupRemap_.end() ? it->second : oldId;
    }

    /** TC-12b: 引用串 "T<id>" → 去重后 "T<newId>"（paramTyIds 文本引用 remap） */
    Str RemapTRef(const Str& ref) const
    {
        if (ref.size() >= 2 && ref[0] == 'T') {
            int id = std::stoi(ref.substr(1));
            int nid = RemapId(id);
            return nid != id ? Str("T") + std::to_string(nid) : ref;
        }
        return ref;
    }

private:
    /**
     * @brief 池等价 key：单例/容器/泛型族转发编译器虚 Hash/==（结构等价）；
     *        Nominal 族（Class/Interface/Struct/Enum）编译器 == 含 declPtr/typeArgs 指针比较（B120RC3
     *        Types.cpp NominalTyEqualTo 实证），跨实例去重需按内容——采用 `kind#pkg#name#typeArgs`
     *        文本 key（设计文档 D-1 修订：Nominal 走文本等价，其余走虚函数转发）。
     */
    struct TyKey {
        const Cangjie::AST::Ty* ty;
        bool useVirtual;    /**< true = 虚 Hash/==；false = 文本 key */
        Str textKey;
    };
    struct TyKeyHash {
        Size operator()(const TyKey& k) const
        {
            return k.useVirtual ? k.ty->Hash() : std::hash<Str>{}(k.textKey);
        }
    };
    struct TyKeyEqual {
        bool operator()(const TyKey& lhs, const TyKey& rhs) const
        {
            if (lhs.useVirtual != rhs.useVirtual) {
                return false;
            }
            if (lhs.useVirtual) {
                return lhs.ty == rhs.ty || *lhs.ty == *rhs.ty;
            }
            return lhs.textKey == rhs.textKey;
        }
    };

    static bool IsNominalKind(Cangjie::AST::TypeKind kind);
    static Str NominalTextKey(const Cangjie::AST::Ty& ty);

    int DoEnsure(const Cangjie::AST::Ty* ty);
    Str Encode(const Cangjie::AST::Ty* ty);

    static Str FmtList(const StrVec& items);

    std::unordered_map<TyKey, int, TyKeyHash, TyKeyEqual> t2id;
    std::unordered_map<const Cangjie::AST::Ty*, FuncDeclMeta> funcMetas; /**< Ty 指针 → decl 上下文值拷贝（仅 func 族；池内持有无悬空） */
    Vec<const Cangjie::AST::Ty*> order; /**< id 顺序的类型指针（行序） */
    StrVec lines;                       /**< T<id>: 行文本 */
    bool resolved{false};               /**< 类型行是否已统一编码 */

    /** TC-12b：去重后 old_id → new_output_id（ResolvedLines 首次调用时构建） */
    std::unordered_map<int, int> dupRemap_;
};

/**
 * @brief func 类型的 decl 上下文元数据（CJAH-5c：编译器 FuncTy 无参数名/默认参数数——
 *        编码 func 类型行时从声明位补齐，对齐 TC names/opt 字段）
 */
struct FuncDeclMeta {
    StrVec paramNames;
    int optionalParamCount{0};
    /** CJAH-6c (N-2)：泛型参数名（FuncDecl.generic→typeParams），补 func 类型行 tp:[...] 段 */
    StrVec typeParamNames;
};

/**
 * @brief 符号行（S<id>@<fileIdx>: <kind>#<name>#ty:T<id>[#params:[[T…]]]）
 */
struct SymRow {
    int id;
    int fileIdx;
    Str kind;
    Str name;
    int tyId;          /**< -1 = 无类型（`-`） */
    StrVec paramTyIds; /**< 每个参数的 T<id> 文本（func/member 签名可比性，CJAH-4b）；空 = 无 */
};

/**
 * @brief 绑定行（<line>:<col>:<identity> -> T<id>）
 */
struct BindRow {
    int line;
    int col;
    Str identity;
    int tyId;
};

/**
 * @brief 绑定收集口径
 */
enum class BindScope {
    ALL,   /**< 全部有语义类型的节点（声明/表达式/块） */
    EXPR   /**< 仅表达式节点（对齐 typechecker typeBindings 口径：RefExpr/MemberAccess/CallExpr/
                BinaryExpr/LitConstExpr/AssignExpr/ReturnExpr/SubscriptExpr 等） */
};

/**
 * @brief R120 v1 全量语义结果序列化 Pass
 *
 * 依赖 SEMA stage 已完成（node.GetTy() 可用）。
 */
class DumpSemanticResultPass : public Pass {
public:
    DumpSemanticResultPass(const ToSourcePassConfig& config) : Pass(config), prt(ofs, 0)
    {
    }
    ~DumpSemanticResultPass() override = default;

    void Run(AstNode& node) override;

private:
    const ToSourcePassConfig& Config() const
    {
        return Cast<const ToSourcePassConfig&>(config);
    }

    void DumpPackage(const Package& pkg);

    void CollectSymbols(const Package& pkg);
    void CollectBindings(const Package& pkg);

    void WriteHeader(const Package& pkg);
    void WriteTypes();
    void WriteSymbols();
    void WriteBindings();

    // 符号收集：文件遍历序分配 S-id
    void CollectFileSymbols(const File& file, int fileIdx);
    void CollectDeclSymbol(const Decl& decl, int fileIdx, bool inExtend = false, bool inTypeLike = false);
    void CollectBodySymbols(const Decl& decl, int fileIdx);
    void CollectBlockLocals(const Block& block, int fileIdx);
    void CollectNestedBlockLocals(const AstNode& stmt, int fileIdx);

    static Str SymKindOf(const Decl& decl);

    std::ofstream ofs;
    Printer prt;
    SemanticTyPool pool;
    BindScope bindScope{BindScope::ALL}; /**< 绑定收集口径（--ser-bind-scope=expr 切换） */
    bool virtualFileKey{false};          /**< CJAH-4d: file 段虚拟名口径（CJAH_SER_FILE_KEY=virtual） */

    // 符号图：S<id>@<fileIdx>（SymRow 文件级 struct 定义见上）
    Vec<SymRow> symRows;

    // 绑定表：(fileIdx, 文件名) → 有序 (line, col, identity, tyId)（按 line/col 冒泡序插入保序）
    Vec<std::pair<Str, Vec<BindRow>>> bindFiles; /**< 每文件一个段 */
};
