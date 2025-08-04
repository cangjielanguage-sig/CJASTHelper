/**
 * @file
 *
 * This file implementation of AstVisitor.
 */

#include "AstVisitor.h"
#include "Logger.h"
#include "cangjie/Utils/CastingTemplate.h"
#include <tuple>

#define GEN_REG_HANDLER(N)                                                                                             \
    registerHandler(                                                                                                   \
        astKinds[#N], [this](const AstNode& node) { return this->Before(Cangjie::StaticCast<const N&>(node)); },       \
        [this](                                                                                                        \
            const AstNode& node, VisitResult& res) { this->VisitChildren(Cangjie::StaticCast<const N&>(node), res); }, \
        [this](                                                                                                        \
            const AstNode& node, const VisitResult& res) { this->After(Cangjie::StaticCast<const N&>(node), res); })

static std::unordered_map<std::string, AstKind> astKinds{
    {"Package", AstKind::PACKAGE},
    {"File", AstKind::FILE},
    {"FuncDecl", AstKind::FUNC_DECL},
    {"FuncBody", AstKind::FUNC_BODY},
    {"FuncParamList", AstKind::FUNC_PARAM_LIST},
    {"FuncParam", AstKind::FUNC_PARAM},
};

AstVisitor::AstVisitor()
{
    EXPAND2(GEN_REG_HANDLER, Package, File);
    EXPAND4(GEN_REG_HANDLER, FuncDecl, FuncBody, FuncParamList, FuncParam);
}

void AstVisitor::registerHandler(AstKind kind, BeforeFunc before, VisitFunc visit, AfterFunc after)
{
    handlers[kind] = {before, visit, after};
    Logger::Get().Debug("AstVisitor::registerHandler", "For ", static_cast<int>(kind));
}

VisitResult AstVisitor::BeforeVisit(const AstNode& node)
{
    // Logger::Get().Debug("AstVisitor::BeforeVisit", "For ", static_cast<int>(node.astKind));
    auto it = handlers.find(node.astKind);
    if (it != handlers.end() && std::get<0>(it->second)) {
        return std::get<0>(it->second)(node);
    } else {
        return DefaultBefore(node);
    }
}

void AstVisitor::VisitChildren(const AstNode& node, VisitResult& visitResult)
{
    auto it = handlers.find(node.astKind);
    if (it != handlers.end() && std::get<1>(it->second)) {
        return std::get<1>(it->second)(node, visitResult);
    } else {
        return DefaultVisitChildren(node, visitResult);
    }
}

void AstVisitor::AfterVisit(const AstNode& node, const VisitResult& visitResult)
{
    // Logger::Get().Debug("AstVisitor::AfterVisit", "For ", static_cast<int>(node.astKind));
    auto it = handlers.find(node.astKind);
    if (it != handlers.end() && std::get<2>(it->second)) {
        return std::get<2>(it->second)(node, visitResult);
    } else {
        return DefaultAfter(node, visitResult);
    }
}

VisitResult AstVisitor::DefaultBefore(const AstNode& node)
{
    return VisitResult::Cont();
}

void AstVisitor::DefaultVisitChildren(const AstNode& node, VisitResult& visitResult)
{
}

void AstVisitor::DefaultAfter(const AstNode& node, const VisitResult& visitResult)
{
}

// VisitChildren
void AstVisitor::VisitChildren(const Package& node, VisitResult& visitResult)
{
    VisitNodes(node.files);
}

void AstVisitor::VisitChildren(const PackageSpec& node, VisitResult& visitResult)
{
    if (node.modifier) {
        traverseAst(*node.modifier, *this);
    }
}

void AstVisitor::VisitChildren(const File& node, VisitResult& visitResult)
{
    VisitNode(node.package);
    VisitNodes(node.imports);
    VisitNodes(node.decls);
}

void AstVisitor::VisitChildren(const FuncDecl& node, VisitResult& visitResult)
{
    VisitNode(node.funcBody);
}

void AstVisitor::VisitChildren(const FuncBody& node, VisitResult& visitResult)
{
    AH_ASSERT(node.paramLists.size() == 1);
    VisitNode(node.paramLists[0]);
    VisitNode(node.generic);
    VisitNode(node.retType);
    VisitNode(node.body);
}

void AstVisitor::VisitChildren(const FuncParamList& node, VisitResult& visitResult)
{
    VisitNodes(node.params);
}

void AstVisitor::VisitChildren(const FuncParam& node, VisitResult& visitResult)
{
    VisitNode(node.assignment);
}

void AstVisitor::VisitChildren(const VarDecl& node, VisitResult& visitResult)
{
    VisitNode(node.type);
}

void AstVisitor::VisitChildren(const ClassDecl& node, VisitResult& visitResult)
{
    VisitNode(node.body);
}

void AstVisitor::VisitChildren(const ClassBody& node, VisitResult& visitResult)
{
    VisitNodes(node.decls);
}

void AstVisitor::VisitChildren(const RefType& node, VisitResult& visitResult)
{
    VisitNodes(node.typeArguments);
}

void AstVisitor::VisitChildren(const FuncType& node, VisitResult& visitResult)
{
    VisitNodes(node.paramTypes);
    VisitNode(node.retType);
}

void AstVisitor::VisitChildren(const Block& node, VisitResult& visitResult)
{
    VisitNodes(node.body);
}

void AstVisitor::VisitChildren(const LitConstExpr& node, VisitResult& visitResult)
{
    VisitNode(node.ref);
    VisitNode(node.siExpr);
}

void AstVisitor::VisitChildren(const ReturnExpr& node, VisitResult& visitResult)
{
    VisitNode(node.expr);
}

void AstVisitor::VisitChildren(const ArrayLit& node, VisitResult& visitResult)
{
    VisitNodes(node.children);
}

void AstVisitor::VisitChildren(const CallExpr& node, VisitResult& visitResult)
{
    VisitNode(node.baseFunc);
    VisitNodes(node.args);
    VisitNodes(node.defaultArgs); // To Check this
}

void AstVisitor::VisitChildren(const FuncArg& node, VisitResult& visitResult)
{
    VisitNode(node.expr);
}

void AstVisitor::VisitChildren(const MemberAccess& node, VisitResult& visitResult)
{
    VisitNode(node.baseExpr);
    VisitNodes(node.typeArguments);
}

void AstVisitor::VisitChildren(const RefExpr& node, VisitResult& visitResult)
{
    VisitNodes(node.typeArguments);
}

void AstVisitor::VisitChildren(const AssignExpr& node, VisitResult& visitResult)
{
    VisitNode(node.leftValue);
    VisitNode(node.rightExpr);
}

void AstVisitor::VisitChildren(const IncOrDecExpr& node, VisitResult& visitResult)
{
    VisitNode(node.expr);
}

void AstVisitor::VisitChildren(const UnaryExpr& node, VisitResult& visitResult)
{
    VisitNode(node.expr);
}

void AstVisitor::VisitChildren(const BinaryExpr& node, VisitResult& visitResult)
{
    VisitNode(node.leftExpr);
    VisitNode(node.rightExpr);
}

void AstVisitor::VisitChildren(const SubscriptExpr& node, VisitResult& visitResult)
{
    VisitNode(node.baseExpr);
    VisitNodes(node.indexExprs);
}
