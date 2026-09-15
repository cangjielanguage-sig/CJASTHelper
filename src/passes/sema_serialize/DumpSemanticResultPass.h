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
     * @brief 按 id 序统一编码类型行（首次调用时执行；子类型引用此时已被分配更大 id，
     *        保证行顺序 = id 顺序——R120 约定 reader 按行序即 id 序索引）
     */
    const StrVec& ResolvedLines()
    {
        if (resolved) {
            return lines;
        }
        resolved = true;
        for (Size i = 0; i < order.size(); i++) {
            lines.push_back("T" + std::to_string(i) + ": " + Encode(order[i]));
        }
        return lines;
    }

    Size Count() const
    {
        return t2id.size();
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
    Vec<const Cangjie::AST::Ty*> order; /**< id 顺序的类型指针（行序） */
    StrVec lines;                       /**< T<id>: 行文本 */
    bool resolved{false};               /**< 类型行是否已统一编码 */
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
    void CollectDeclSymbol(const Decl& decl, int fileIdx);
    void CollectBodySymbols(const Decl& decl, int fileIdx);

    static Str SymKindOf(const Decl& decl);
    static Str IdentityOf(const AstNode& node);

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
