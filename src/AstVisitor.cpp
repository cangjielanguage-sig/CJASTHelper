/**
 * @file
 *
 * This file implementation of AstVisitor.
 */

#include "AstVisitor.h"
#include "Logger.h"
#include "Macro.h"
#include "cangjie/Utils/CastingTemplate.h"
#include <tuple>

// AstKind 2 String
static std::unordered_map<AstKind, std::string> kindsInfo{
#define AST_INFO(KIND, STR, DEF) {AstKind::KIND, STR},
#include "AstInfo.inc"
#undef AST_INFO
};

AstVisitor::AstVisitor()
{
// 定义注册代码片段
#define GEN_REG_HANDLER(KIND, N)                                                                                       \
    registerHandler(                                                                                                   \
        AstKind::KIND, [this](const AstNode& node) { return this->Before(Cangjie::StaticCast<const N&>(node)); },      \
        [this](                                                                                                        \
            const AstNode& node, VisitResult& res) { this->VisitChildren(Cangjie::StaticCast<const N&>(node), res); }, \
        [this](                                                                                                        \
            const AstNode& node, const VisitResult& res) { this->After(Cangjie::StaticCast<const N&>(node), res); })
// 使用宏生成代码
#define AST_INFO(KIND, STR, DEF) GEN_REG_HANDLER(KIND, DEF);
#include "AstInfo.inc"
#undef AST_INFO
}

void AstVisitor::registerHandler(AstKind kind, BeforeFunc before, VisitFunc visit, AfterFunc after)
{
    handlers[kind] = {before, visit, after};
    Logger::Get().Debug("AstVisitor::registerHandler", "For ", kindsInfo[kind]);
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
// 定义默认实现宏
#define GEN_VISIT_CHIRLDREN_DEFAULT_IMPL(N)                                                                            \
    void AstVisitor::VisitChildren(const N& node, VisitResult& visitResult)                                            \
    {                                                                                                                  \
    }
// 递归展开宏
EXPAND2(GEN_VISIT_CHIRLDREN_DEFAULT_IMPL, Modifier, Annotation);
EXPAND2(GEN_VISIT_CHIRLDREN_DEFAULT_IMPL, PrimitiveType, ThisType);
EXPAND4(GEN_VISIT_CHIRLDREN_DEFAULT_IMPL, WildcardPattern, WildcardExpr, PrimitiveTypeExpr, JumpExpr);
EXPAND1(GEN_VISIT_CHIRLDREN_DEFAULT_IMPL, MacroExpandParam); // To check

void AstVisitor::VisitChildren(const ImportContent& node, VisitResult& visitResult)
{
    for (auto& ic : node.items) {
        traverseAst(ic, *this);
    }
}

void AstVisitor::VisitChildren(const ImportSpec& node, VisitResult& visitResult)
{
    VisitNode(node.modifier);
    VisitChildren(node.content, visitResult);
}

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
    Logger::Get().Debug("AstVisitor::VisitChildren", "For FuncDecl: ", node.identifier.Val());
    VisitDecl(node, visitResult);
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
    VisitDecl(node, visitResult);
    VisitNode(node.assignment);
}

void AstVisitor::VisitChildren(const VarDecl& node, VisitResult& visitResult)
{
    VisitDecl(node, visitResult);
    VisitNode(node.type);
}

void AstVisitor::VisitChildren(const ClassDecl& node, VisitResult& visitResult)
{
    VisitDecl(node, visitResult);
    VisitNodes(node.inheritedTypes);
    VisitNode(node.body);
}

void AstVisitor::VisitChildren(const ClassBody& node, VisitResult& visitResult)
{
    VisitNodes(node.decls);
}

void AstVisitor::VisitChildren(const PrimaryCtorDecl& node, VisitResult& visitResult)
{
    VisitDecl(node, visitResult);
    VisitNode(node.funcBody);
}

void AstVisitor::VisitChildren(const StructDecl& node, VisitResult& visitResult)
{
    VisitDecl(node, visitResult);
    VisitNodes(node.inheritedTypes);
    VisitNode(node.body);
}

void AstVisitor::VisitChildren(const StructBody& node, VisitResult& visitResult)
{
    VisitNodes(node.decls);
}

void AstVisitor::VisitChildren(const InterfaceDecl& node, VisitResult& visitResult)
{
    VisitDecl(node, visitResult);
    VisitNodes(node.inheritedTypes);
    VisitNode(node.body);
}

void AstVisitor::VisitChildren(const InterfaceBody& node, VisitResult& visitResult)
{
    VisitNodes(node.decls);
}

void AstVisitor::VisitChildren(const EnumDecl& node, VisitResult& visitResult)
{
    VisitDecl(node, visitResult);
    VisitNodes(node.inheritedTypes);
    VisitNodes(node.constructors);
    VisitNodes(node.members);
}

void AstVisitor::VisitChildren(const ExtendDecl& node, VisitResult& visitResult)
{
    VisitDecl(node, visitResult);
    VisitNode(node.extendedType);
    VisitNodes(node.inheritedTypes);
    VisitNodes(node.members);
}

void AstVisitor::VisitChildren(const PropDecl& node, VisitResult& visitResult)
{
    VisitDecl(node, visitResult);
    VisitNodes(node.getters);
    VisitNodes(node.setters);
}

void AstVisitor::VisitChildren(const VarWithPatternDecl& node, VisitResult& visitResult)
{
    VisitDecl(node, visitResult);
    VisitNode(node.irrefutablePattern);
}

void AstVisitor::VisitChildren(const MainDecl& node, VisitResult& visitResult)
{
    VisitDecl(node, visitResult);
    VisitNode(node.funcBody);
    // Desugar Decl?
}

void AstVisitor::VisitChildren(const TypeAliasDecl& node, VisitResult& visitResult)
{
    VisitDecl(node, visitResult);
    VisitNode(node.type);
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

void AstVisitor::VisitChildren(const VArrayType& node, VisitResult& visitResult)
{
    VisitNode(node.typeArgument);
    VisitNode(node.constantType);
}

void AstVisitor::VisitChildren(const ConstantType& node, VisitResult& visitResult)
{
    VisitNode(node.constantExpr);
}

void AstVisitor::VisitChildren(const ParenType& node, VisitResult& visitResult)
{
    VisitNode(node.type);
}

void AstVisitor::VisitChildren(const OptionType& node, VisitResult& visitResult)
{
    VisitNode(node.componentType);
}

void AstVisitor::VisitChildren(const TupleType& node, VisitResult& visitResult)
{
    VisitNodes(node.fieldTypes);
}

void AstVisitor::VisitChildren(const QualifiedType& node, VisitResult& visitResult)
{
    VisitNode(node.baseType);
    VisitNodes(node.typeArguments);
}

// Patterns
void AstVisitor::VisitChildren(const ConstPattern& node, VisitResult& visitResult)
{
    VisitNode(node.literal);
    VisitNode(node.operatorCallExpr);
}

void AstVisitor::VisitChildren(const TypePattern& node, VisitResult& visitResult)
{
    VisitNode(node.pattern);
    VisitNode(node.type);
    // desugar?
    VisitNode(node.desugarExpr);
    VisitNode(node.desugarVarPattern);
}

void AstVisitor::VisitChildren(const VarPattern& node, VisitResult& visitResult)
{
    VisitNode(node.varDecl);
    VisitNode(node.desugarExpr); // desugar?
}

void AstVisitor::VisitChildren(const TuplePattern& node, VisitResult& visitResult)
{
    VisitNodes(node.patterns);
}

void AstVisitor::VisitChildren(const EnumPattern& node, VisitResult& visitResult)
{
    VisitNode(node.constructor);
    VisitNodes(node.patterns);
}

void AstVisitor::VisitChildren(const VarOrEnumPattern& node, VisitResult& visitResult)
{
    VisitNode(node.pattern);
}

void AstVisitor::VisitChildren(const ExceptTypePattern& node, VisitResult& visitResult)
{
    VisitNode(node.pattern);
    VisitNodes(node.types);
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

void AstVisitor::VisitChildren(const PointerExpr& node, VisitResult& visitResult)
{
    VisitNode(node.type);
    VisitNode(node.arg);
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

void AstVisitor::VisitChildren(const IfExpr& node, VisitResult& visitResult)
{
    VisitNode(node.condExpr);
    VisitNode(node.thenBody);
    VisitNode(node.elseBody);
}

void AstVisitor::VisitChildren(const DoWhileExpr& node, VisitResult& visitResult)
{
    VisitNode(node.body);
    VisitNode(node.condExpr);
}

void AstVisitor::VisitChildren(const WhileExpr& node, VisitResult& visitResult)
{
    VisitNode(node.condExpr);
    VisitNode(node.body);
}

void AstVisitor::VisitChildren(const ForInExpr& node, VisitResult& visitResult)
{
    VisitNode(node.pattern);
    VisitNode(node.patternGuard);
    VisitNode(node.inExpression);
    VisitNode(node.body);
}

void AstVisitor::VisitChildren(const LetPatternDestructor& node, VisitResult& visitResult)
{
    VisitNodes(node.patterns);
    VisitNode(node.initializer);
}

void AstVisitor::VisitChildren(const OptionalExpr& node, VisitResult& visitResult)
{
    VisitNode(node.baseExpr);
}

void AstVisitor::VisitChildren(const OptionalChainExpr& node, VisitResult& visitResult)
{
    VisitNode(node.expr);
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

void AstVisitor::VisitChildren(const TypeConvExpr& node, VisitResult& visitResult)
{
    VisitNode(node.type);
    VisitNode(node.expr);
}

void AstVisitor::VisitChildren(const ParenExpr& node, VisitResult& visitResult)
{
    VisitNode(node.expr);
}

void AstVisitor::VisitChildren(const IsExpr& node, VisitResult& visitResult)
{
    VisitNode(node.leftExpr);
    VisitNode(node.isType);
}

void AstVisitor::VisitChildren(const AsExpr& node, VisitResult& visitResult)
{
    VisitNode(node.leftExpr);
    VisitNode(node.asType);
}

void AstVisitor::VisitChildren(const RangeExpr& node, VisitResult& visitResult)
{
    VisitNode(node.startExpr);
    VisitNode(node.stopExpr);
    VisitNode(node.stepExpr);
}

void AstVisitor::VisitChildren(const ArrayExpr& node, VisitResult& visitResult)
{
    VisitNode(node.type);
    VisitNodes(node.args);
}

void AstVisitor::VisitChildren(const LambdaExpr& node, VisitResult& visitResult)
{
    VisitNode(node.funcBody);
}

void AstVisitor::VisitChildren(const TrailingClosureExpr& node, VisitResult& visitResult)
{
    VisitNode(node.expr);
    VisitNode(node.lambda);
}

void AstVisitor::VisitChildren(const TryExpr& node, VisitResult& visitResult)
{
    VisitNodes(node.resourceSpec);
    VisitNode(node.tryBlock);
    VisitNodes(node.catchPatterns);
    VisitNodes(node.catchBlocks);
    VisitNode(node.finallyBlock);
}

void AstVisitor::VisitChildren(const ThrowExpr& node, VisitResult& visitResult)
{
    VisitNode(node.expr);
}

void AstVisitor::VisitChildren(const MatchCase& node, VisitResult& visitResult)
{
    VisitNodes(node.patterns);
    VisitNode(node.patternGuard);
    VisitNode(node.exprOrDecls);
}

void AstVisitor::VisitChildren(const MatchCaseOther& node, VisitResult& visitResult)
{
    VisitNode(node.matchExpr);
    VisitNode(node.exprOrDecls);
}

void AstVisitor::VisitChildren(const MatchExpr& node, VisitResult& visitResult)
{
    VisitNode(node.selector);
    VisitNodes(node.matchCases);
    VisitNodes(node.matchCaseOthers);
}

void AstVisitor::VisitChildren(const TupleLit& node, VisitResult& visitResult)
{
    VisitNodes(node.children);
}

void AstVisitor::VisitChildren(const ArrayLit& node, VisitResult& visitResult)
{
    VisitNodes(node.children);
}

void AstVisitor::VisitChildren(const SpawnExpr& node, VisitResult& visitResult)
{
    VisitNode(node.futureObj);
    VisitNode(node.task);
    VisitNode(node.arg);
}

void AstVisitor::VisitChildren(const SynchronizedExpr& node, VisitResult& visitResult)
{
    VisitNode(node.mutex);
    VisitNode(node.body);
}

// Generic
void AstVisitor::VisitChildren(const GenericConstraint& node, VisitResult& visitResult)
{
    VisitNode(node.type);
    VisitNodes(node.upperBounds);
}

void AstVisitor::VisitChildren(const Generic& node, VisitResult& visitResult)
{
    VisitNodes(node.typeParameters);
    VisitNodes(node.genericConstraints);
}

void AstVisitor::VisitChildren(const GenericParamDecl& node, VisitResult& visitResult)
{
    VisitDecl(node, visitResult);
}

// Macro
void AstVisitor::VisitChildren(const MacroDecl& node, VisitResult& visitResult)
{
    VisitDecl(node, visitResult);
    VisitNode(node.funcBody);
    // Desugared Func?
}

void AstVisitor::VisitChildren(const MacroExpandDecl& node, VisitResult& visitResult)
{
    // To Check
    VisitDecl(node, visitResult);
    VisitNode(node.invocation.decl);
    VisitNodes(node.invocation.nodes);
}

void AstVisitor::VisitChildren(const TokenPart& node, VisitResult& visitResult)
{
}

void AstVisitor::VisitChildren(const QuoteExpr& node, VisitResult& visitResult)
{
    VisitNodes(node.exprs);
}

void AstVisitor::VisitChildren(const MacroExpandExpr& node, VisitResult& visitResult)
{
    VisitNodes(node.annotations);
    for (auto mod : node.modifiers) {
        VisitChildren(mod, visitResult);
    }
}

void AstVisitor::VisitChildren(const InterpolationExpr& node, VisitResult& visitResult)
{
    VisitNode(node.block);
}

void AstVisitor::VisitChildren(const StrInterpolationExpr& node, VisitResult& visitResult)
{
    VisitNodes(node.strPartExprs);
}

void AstVisitor::VisitChildren(const BuiltInDecl& node, VisitResult& visitResult)
{
    // TO Check
    VisitChildren(node, visitResult);
}

void AstVisitor::VisitDecl(const Decl& node, VisitResult& visitResult)
{
    for (auto& mod : node.modifiers) {
        VisitChildren(mod, visitResult);
    }
    VisitNodes(node.annotations);
    VisitNode(node.annotationsArray);
    VisitNode(node.generic);
}
