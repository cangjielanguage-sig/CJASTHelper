/**
 * @file
 *
 * This file declares the generic AstVisitor.
 */

#ifndef AST_VISITOR_H
#define AST_VISITOR_H

#include "AstVisitorBase.h"
#include <functional>
#include <map>
#include <tuple>

using AstKind = Cangjie::AST::ASTKind;
using Cangjie::AST::Decl;

// 宏自动生成 using Cangjie::AST::Package
#define AST_INFO(KIND, STR, DEF) using Cangjie::AST::DEF;
#include "AstInfo.inc"
#undef AST_INFO

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

    // 宏定义
#define GEN_BEFORE_AFTER_VISIT_CHILDREN(N)                                                                             \
    virtual VisitResult Before(const N& node)                                                                          \
    {                                                                                                                  \
        return VisitResult::Cont();                                                                                    \
    }                                                                                                                  \
    virtual void After(const N& node, const VisitResult& visitResult)                                                  \
    {                                                                                                                  \
    }                                                                                                                  \
    virtual void VisitChildren(const N& node, VisitResult& visitResult);

    // 使用宏生成代码
#define AST_INFO(KIND, STR, DEF) GEN_BEFORE_AFTER_VISIT_CHILDREN(DEF)
#include "AstInfo.inc"
#undef AST_INFO

protected:
    std::map<AstKind, std::tuple<BeforeFunc, VisitFunc, AfterFunc>> handlers;

private:
    void VisitDecl(const Decl& node, VisitResult& visitResult);

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
