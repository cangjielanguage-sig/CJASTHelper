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
        [this](const AstNode& node, VisitResult& res) { this->Visit(Cangjie::StaticCast<const N&>(node), res); },      \
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

void AstVisitor::Visit(const AstNode& node, VisitResult& res)
{
    auto it = handlers.find(node.astKind);
    if (it != handlers.end() && std::get<1>(it->second)) {
        return std::get<1>(it->second)(node, res);
    } else {
        return DefaultVisit(node, res);
    }
}

void AstVisitor::AfterVisit(const AstNode& node, const VisitResult& res)
{
    // Logger::Get().Debug("AstVisitor::AfterVisit", "For ", static_cast<int>(node.astKind));
    auto it = handlers.find(node.astKind);
    if (it != handlers.end() && std::get<2>(it->second)) {
        return std::get<2>(it->second)(node, res);
    } else {
        return DefaultAfter(node, res);
    }
}

VisitResult AstVisitor::DefaultBefore(const AstNode& node)
{
    return VisitResult::Cont();
}

void AstVisitor::DefaultVisit(const AstNode& node, VisitResult& res)
{
}

void AstVisitor::DefaultAfter(const AstNode& node, const VisitResult& res)
{
}

// Visit
// 定义默认实现宏
#define GEN_VISIT_DEFAULT_IMPL(N)                                                                                      \
    void AstVisitor::Visit(const N& node, VisitResult& res)                                                            \
    {                                                                                                                  \
    }
// 递归展开宏
EXPAND2(GEN_VISIT_DEFAULT_IMPL, Modifier, Annotation);
EXPAND2(GEN_VISIT_DEFAULT_IMPL, PrimitiveType, ThisType);
EXPAND4(GEN_VISIT_DEFAULT_IMPL, WildcardPattern, WildcardExpr, PrimitiveTypeExpr, JumpExpr);
EXPAND1(GEN_VISIT_DEFAULT_IMPL, MacroExpandParam); // To check

void AstVisitor::Visit(const ImportContent& node, VisitResult& res)
{
    for (auto& ic : node.items) {
        traverseAst(ic, *this);
    }
}

void AstVisitor::Visit(const ImportSpec& node, VisitResult& res)
{
    VisitNode(node.modifier);
    Visit(node.content, res);
}

void AstVisitor::Visit(const Package& node, VisitResult& res)
{
    VisitNodes(node.files);
}

void AstVisitor::Visit(const PackageSpec& node, VisitResult& res)
{
    if (node.modifier) {
        traverseAst(*node.modifier, *this);
    }
}

void AstVisitor::Visit(const File& node, VisitResult& res)
{
    VisitNode(node.package);
    VisitNodes(node.imports);
    VisitNodes(node.decls);
}

void AstVisitor::Visit(const FuncDecl& node, VisitResult& res)
{
    Logger::Get().Debug("AstVisitor::Visit", "For FuncDecl: ", node.identifier.Val());
    VisitDecl(node, res);
    VisitNode(node.funcBody);
}

void AstVisitor::Visit(const FuncBody& node, VisitResult& res)
{
    AH_ASSERT(node.paramLists.size() == 1);
    VisitNode(node.paramLists[0]);
    VisitNode(node.generic);
    VisitNode(node.retType);
    VisitNode(node.body);
}

void AstVisitor::Visit(const FuncParamList& node, VisitResult& res)
{
    VisitNodes(node.params);
}

void AstVisitor::Visit(const FuncParam& node, VisitResult& res)
{
    VisitDecl(node, res);
    VisitNode(node.assignment);
}

void AstVisitor::Visit(const VarDecl& node, VisitResult& res)
{
    VisitDecl(node, res);
    VisitNode(node.type);
}

void AstVisitor::Visit(const ClassDecl& node, VisitResult& res)
{
    VisitDecl(node, res);
    VisitNodes(node.inheritedTypes);
    VisitNode(node.body);
}

void AstVisitor::Visit(const ClassBody& node, VisitResult& res)
{
    VisitNodes(node.decls);
}

void AstVisitor::Visit(const PrimaryCtorDecl& node, VisitResult& res)
{
    VisitDecl(node, res);
    VisitNode(node.funcBody);
}

void AstVisitor::Visit(const StructDecl& node, VisitResult& res)
{
    VisitDecl(node, res);
    VisitNodes(node.inheritedTypes);
    VisitNode(node.body);
}

void AstVisitor::Visit(const StructBody& node, VisitResult& res)
{
    VisitNodes(node.decls);
}

void AstVisitor::Visit(const InterfaceDecl& node, VisitResult& res)
{
    VisitDecl(node, res);
    VisitNodes(node.inheritedTypes);
    VisitNode(node.body);
}

void AstVisitor::Visit(const InterfaceBody& node, VisitResult& res)
{
    VisitNodes(node.decls);
}

void AstVisitor::Visit(const EnumDecl& node, VisitResult& res)
{
    VisitDecl(node, res);
    VisitNodes(node.inheritedTypes);
    VisitNodes(node.constructors);
    VisitNodes(node.members);
}

void AstVisitor::Visit(const ExtendDecl& node, VisitResult& res)
{
    VisitDecl(node, res);
    VisitNode(node.extendedType);
    VisitNodes(node.inheritedTypes);
    VisitNodes(node.members);
}

void AstVisitor::Visit(const PropDecl& node, VisitResult& res)
{
    VisitDecl(node, res);
    VisitNodes(node.getters);
    VisitNodes(node.setters);
}

void AstVisitor::Visit(const VarWithPatternDecl& node, VisitResult& res)
{
    VisitDecl(node, res);
    VisitNode(node.irrefutablePattern);
}

void AstVisitor::Visit(const MainDecl& node, VisitResult& res)
{
    VisitDecl(node, res);
    VisitNode(node.funcBody);
    // Desugar Decl?
}

void AstVisitor::Visit(const TypeAliasDecl& node, VisitResult& res)
{
    VisitDecl(node, res);
    VisitNode(node.type);
}

void AstVisitor::Visit(const RefType& node, VisitResult& res)
{
    VisitNodes(node.typeArguments);
}

void AstVisitor::Visit(const FuncType& node, VisitResult& res)
{
    VisitNodes(node.paramTypes);
    VisitNode(node.retType);
}

void AstVisitor::Visit(const VArrayType& node, VisitResult& res)
{
    VisitNode(node.typeArgument);
    VisitNode(node.constantType);
}

void AstVisitor::Visit(const ConstantType& node, VisitResult& res)
{
    VisitNode(node.constantExpr);
}

void AstVisitor::Visit(const ParenType& node, VisitResult& res)
{
    VisitNode(node.type);
}

void AstVisitor::Visit(const OptionType& node, VisitResult& res)
{
    VisitNode(node.componentType);
}

void AstVisitor::Visit(const TupleType& node, VisitResult& res)
{
    VisitNodes(node.fieldTypes);
}

void AstVisitor::Visit(const QualifiedType& node, VisitResult& res)
{
    VisitNode(node.baseType);
    VisitNodes(node.typeArguments);
}

// Patterns
void AstVisitor::Visit(const ConstPattern& node, VisitResult& res)
{
    VisitNode(node.literal);
    VisitNode(node.operatorCallExpr);
}

void AstVisitor::Visit(const TypePattern& node, VisitResult& res)
{
    VisitNode(node.pattern);
    VisitNode(node.type);
    // desugar?
    VisitNode(node.desugarExpr);
    VisitNode(node.desugarVarPattern);
}

void AstVisitor::Visit(const VarPattern& node, VisitResult& res)
{
    VisitNode(node.varDecl);
    VisitNode(node.desugarExpr); // desugar?
}

void AstVisitor::Visit(const TuplePattern& node, VisitResult& res)
{
    VisitNodes(node.patterns);
}

void AstVisitor::Visit(const EnumPattern& node, VisitResult& res)
{
    VisitNode(node.constructor);
    VisitNodes(node.patterns);
}

void AstVisitor::Visit(const VarOrEnumPattern& node, VisitResult& res)
{
    VisitNode(node.pattern);
}

void AstVisitor::Visit(const ExceptTypePattern& node, VisitResult& res)
{
    VisitNode(node.pattern);
    VisitNodes(node.types);
}

void AstVisitor::Visit(const Block& node, VisitResult& res)
{
    VisitNodes(node.body);
}

void AstVisitor::Visit(const LitConstExpr& node, VisitResult& res)
{
    VisitNode(node.ref);
    VisitNode(node.siExpr);
}

void AstVisitor::Visit(const ReturnExpr& node, VisitResult& res)
{
    VisitNode(node.expr);
}

void AstVisitor::Visit(const PointerExpr& node, VisitResult& res)
{
    VisitNode(node.type);
    VisitNode(node.arg);
}

void AstVisitor::Visit(const CallExpr& node, VisitResult& res)
{
    VisitNode(node.baseFunc);
    VisitNodes(node.args);
    VisitNodes(node.defaultArgs); // To Check this
}

void AstVisitor::Visit(const FuncArg& node, VisitResult& res)
{
    VisitNode(node.expr);
}

void AstVisitor::Visit(const MemberAccess& node, VisitResult& res)
{
    VisitNode(node.baseExpr);
    VisitNodes(node.typeArguments);
}

void AstVisitor::Visit(const RefExpr& node, VisitResult& res)
{
    VisitNodes(node.typeArguments);
}

void AstVisitor::Visit(const IfExpr& node, VisitResult& res)
{
    VisitNode(node.condExpr);
    VisitNode(node.thenBody);
    VisitNode(node.elseBody);
}

void AstVisitor::Visit(const DoWhileExpr& node, VisitResult& res)
{
    VisitNode(node.body);
    VisitNode(node.condExpr);
}

void AstVisitor::Visit(const WhileExpr& node, VisitResult& res)
{
    VisitNode(node.condExpr);
    VisitNode(node.body);
}

void AstVisitor::Visit(const ForInExpr& node, VisitResult& res)
{
    VisitNode(node.pattern);
    VisitNode(node.patternGuard);
    VisitNode(node.inExpression);
    VisitNode(node.body);
}

void AstVisitor::Visit(const LetPatternDestructor& node, VisitResult& res)
{
    VisitNodes(node.patterns);
    VisitNode(node.initializer);
}

void AstVisitor::Visit(const OptionalExpr& node, VisitResult& res)
{
    VisitNode(node.baseExpr);
}

void AstVisitor::Visit(const OptionalChainExpr& node, VisitResult& res)
{
    VisitNode(node.expr);
}

void AstVisitor::Visit(const AssignExpr& node, VisitResult& res)
{
    VisitNode(node.leftValue);
    VisitNode(node.rightExpr);
}

void AstVisitor::Visit(const IncOrDecExpr& node, VisitResult& res)
{
    VisitNode(node.expr);
}

void AstVisitor::Visit(const UnaryExpr& node, VisitResult& res)
{
    VisitNode(node.expr);
}

void AstVisitor::Visit(const BinaryExpr& node, VisitResult& res)
{
    VisitNode(node.leftExpr);
    VisitNode(node.rightExpr);
}

void AstVisitor::Visit(const SubscriptExpr& node, VisitResult& res)
{
    VisitNode(node.baseExpr);
    VisitNodes(node.indexExprs);
}

void AstVisitor::Visit(const TypeConvExpr& node, VisitResult& res)
{
    VisitNode(node.type);
    VisitNode(node.expr);
}

void AstVisitor::Visit(const ParenExpr& node, VisitResult& res)
{
    VisitNode(node.expr);
}

void AstVisitor::Visit(const IsExpr& node, VisitResult& res)
{
    VisitNode(node.leftExpr);
    VisitNode(node.isType);
}

void AstVisitor::Visit(const AsExpr& node, VisitResult& res)
{
    VisitNode(node.leftExpr);
    VisitNode(node.asType);
}

void AstVisitor::Visit(const RangeExpr& node, VisitResult& res)
{
    VisitNode(node.startExpr);
    VisitNode(node.stopExpr);
    VisitNode(node.stepExpr);
}

void AstVisitor::Visit(const ArrayExpr& node, VisitResult& res)
{
    VisitNode(node.type);
    VisitNodes(node.args);
}

void AstVisitor::Visit(const LambdaExpr& node, VisitResult& res)
{
    VisitNode(node.funcBody);
}

void AstVisitor::Visit(const TrailingClosureExpr& node, VisitResult& res)
{
    VisitNode(node.expr);
    VisitNode(node.lambda);
}

void AstVisitor::Visit(const TryExpr& node, VisitResult& res)
{
    VisitNodes(node.resourceSpec);
    VisitNode(node.tryBlock);
    VisitNodes(node.catchPatterns);
    VisitNodes(node.catchBlocks);
    VisitNode(node.finallyBlock);
}

void AstVisitor::Visit(const ThrowExpr& node, VisitResult& res)
{
    VisitNode(node.expr);
}

void AstVisitor::Visit(const MatchCase& node, VisitResult& res)
{
    VisitNodes(node.patterns);
    VisitNode(node.patternGuard);
    VisitNode(node.exprOrDecls);
}

void AstVisitor::Visit(const MatchCaseOther& node, VisitResult& res)
{
    VisitNode(node.matchExpr);
    VisitNode(node.exprOrDecls);
}

void AstVisitor::Visit(const MatchExpr& node, VisitResult& res)
{
    VisitNode(node.selector);
    VisitNodes(node.matchCases);
    VisitNodes(node.matchCaseOthers);
}

void AstVisitor::Visit(const TupleLit& node, VisitResult& res)
{
    VisitNodes(node.children);
}

void AstVisitor::Visit(const ArrayLit& node, VisitResult& res)
{
    VisitNodes(node.children);
}

void AstVisitor::Visit(const SpawnExpr& node, VisitResult& res)
{
    VisitNode(node.futureObj);
    VisitNode(node.task);
    VisitNode(node.arg);
}

void AstVisitor::Visit(const SynchronizedExpr& node, VisitResult& res)
{
    VisitNode(node.mutex);
    VisitNode(node.body);
}

// Generic
void AstVisitor::Visit(const GenericConstraint& node, VisitResult& res)
{
    VisitNode(node.type);
    VisitNodes(node.upperBounds);
}

void AstVisitor::Visit(const Generic& node, VisitResult& res)
{
    VisitNodes(node.typeParameters);
    VisitNodes(node.genericConstraints);
}

void AstVisitor::Visit(const GenericParamDecl& node, VisitResult& res)
{
    VisitDecl(node, res);
}

// Macro
void AstVisitor::Visit(const MacroDecl& node, VisitResult& res)
{
    VisitDecl(node, res);
    VisitNode(node.funcBody);
    // Desugared Func?
}

void AstVisitor::Visit(const MacroExpandDecl& node, VisitResult& res)
{
    // To Check
    VisitDecl(node, res);
    VisitNode(node.invocation.decl);
    VisitNodes(node.invocation.nodes);
}

void AstVisitor::Visit(const TokenPart& node, VisitResult& res)
{
}

void AstVisitor::Visit(const QuoteExpr& node, VisitResult& res)
{
    VisitNodes(node.exprs);
}

void AstVisitor::Visit(const MacroExpandExpr& node, VisitResult& res)
{
    VisitNodes(node.annotations);
    for (auto mod : node.modifiers) {
        Visit(mod, res);
    }
}

void AstVisitor::Visit(const InterpolationExpr& node, VisitResult& res)
{
    VisitNode(node.block);
}

void AstVisitor::Visit(const StrInterpolationExpr& node, VisitResult& res)
{
    VisitNodes(node.strPartExprs);
}

void AstVisitor::Visit(const BuiltInDecl& node, VisitResult& res)
{
    // TO Check
    Visit(node, res);
}

void AstVisitor::VisitDecl(const Decl& node, VisitResult& res)
{
    VisitNodes(node.annotations);
    VisitNode(node.annotationsArray);
    for (auto& mod : node.modifiers) {
        Visit(mod, res);
    }
    VisitNode(node.generic);
}
