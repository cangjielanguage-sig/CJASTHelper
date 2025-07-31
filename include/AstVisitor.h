/**
 * @file
 *
 * This file declares the generic AstVisitor.
 */

#ifndef AST_VISITOR_H
#define AST_VISITOR_H

#include "AstVisitorBase.h"
#include "Macro.h"
#include <functional>
#include <map>
#include <tuple>
// 宏定义
#define GEN_BEFORE_AFTER_VISIT_0(N)                                                                                    \
    virtual VisitResult Before(const N& node)                                                                          \
    {                                                                                                                  \
        return VisitResult::Cont();                                                                                    \
    }                                                                                                                  \
    virtual void After(const N& node, const VisitResult& visitResult)                                                  \
    {                                                                                                                  \
    }

#define GEN_BEFORE_AFTER_CHILDREN_VISIT(N)                                                                             \
    GEN_BEFORE_AFTER_VISIT_0(N)                                                                                        \
    virtual void VisitChildren(const N& node, VisitResult& visitResult)

#define GEN_BEFORE_AFTER_DEFAULT_CHILDREN_VISIT(N)                                                                     \
    GEN_BEFORE_AFTER_VISIT_0(N)                                                                                        \
    virtual void VisitChildren(const N& node, VisitResult& visitResult)                                                \
    {                                                                                                                  \
    }

#define GEN_USING_N(N) using Cangjie::AST::N

using AstKind = Cangjie::AST::ASTKind;
// 宏自动生成 using Cangjie::AST::Package
EXPAND2(GEN_USING_N, Modifier, Annotation);
EXPAND4(GEN_USING_N, Package, PackageSpec, ImportSpec, File);
EXPAND4(GEN_USING_N, FuncDecl, FuncBody, FuncParamList, FuncParam);
EXPAND3(GEN_USING_N, VarDecl, ClassDecl, ClassBody);
EXPAND4(GEN_USING_N, PrimitiveType, RefType, FuncType, ThisType);
EXPAND4(GEN_USING_N, Block, LitConstExpr, ReturnExpr, ArrayLit);
EXPAND4(GEN_USING_N, CallExpr, FuncArg, MemberAccess, RefExpr);
EXPAND4(GEN_USING_N, AssignExpr, IncOrDecExpr, UnaryExpr, BinaryExpr);
EXPAND1(GEN_USING_N, SubscriptExpr);

class AstVisitor : public AstVisitorBase {
public:
    using BeforeFunc = std::function<VisitResult(const AstNode&)>;
    using VisitFunc = std::function<void(const AstNode&, VisitResult&)>;
    using AfterFunc = std::function<void(const AstNode&, const VisitResult&)>;

public:
    AstVisitor();
    void registerHandler(AstKind kind, BeforeFunc before, VisitFunc visit = nullptr, AfterFunc after = nullptr);

    VisitResult BeforeVisit(const AstNode& node) override;
    void VisitChildren(const AstNode& node, VisitResult& visitResult) override;
    void AfterVisit(const AstNode& node, const VisitResult& visitResult) override;

    virtual VisitResult DefaultBefore(const AstNode& node);
    virtual void DefaultVisitChildren(const AstNode& node, VisitResult& visitResult);
    virtual void DefaultAfter(const AstNode& node, const VisitResult& visitResult);

    // 使用宏生成代码
    EXPAND2(GEN_BEFORE_AFTER_DEFAULT_CHILDREN_VISIT, Modifier, Annotation);
    EXPAND2(GEN_BEFORE_AFTER_DEFAULT_CHILDREN_VISIT, PrimitiveType, ThisType);

    EXPAND2(GEN_BEFORE_AFTER_CHILDREN_VISIT, Package, PackageSpec);
    EXPAND1(GEN_BEFORE_AFTER_CHILDREN_VISIT, File);
    EXPAND4(GEN_BEFORE_AFTER_CHILDREN_VISIT, FuncDecl, FuncBody, FuncParamList, FuncParam);
    EXPAND3(GEN_BEFORE_AFTER_CHILDREN_VISIT, VarDecl, ClassDecl, ClassBody);
    EXPAND2(GEN_BEFORE_AFTER_CHILDREN_VISIT, RefType, FuncType);
    EXPAND4(GEN_BEFORE_AFTER_CHILDREN_VISIT, Block, LitConstExpr, ReturnExpr, ArrayLit);
    EXPAND4(GEN_BEFORE_AFTER_CHILDREN_VISIT, CallExpr, FuncArg, MemberAccess, RefExpr);
    EXPAND4(GEN_BEFORE_AFTER_CHILDREN_VISIT, AssignExpr, IncOrDecExpr, UnaryExpr, BinaryExpr);
    EXPAND1(GEN_BEFORE_AFTER_CHILDREN_VISIT, SubscriptExpr);

protected:
    std::map<AstKind, std::tuple<BeforeFunc, VisitFunc, AfterFunc>> handlers;

private:
    template <template <typename> class Ptr, typename T> inline void VisitNode(const Ptr<T>& pnode)
    {
        if (pnode) {
            traverseAst(*pnode, *this);
        }
    }

    template <template <typename> class Ptr, typename T> inline void VisitNodes(const std::vector<Ptr<T>>& nodes)
    {
        for (auto& node : nodes) {
            traverseAst(*node, *this);
        }
    }
};

#endif // AST_VISITOR_H
