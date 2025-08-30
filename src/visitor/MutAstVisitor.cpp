/**
 * @file
 *
 * This file implementation of MutAstVisitor.
 */

#include "visitor/MutAstVisitor.h"
#include "cangjie/Utils/CastingTemplate.h"
#include "utils/Logger.h"
#include "utils/Macro.h"
#include <tuple>

// AstKind 2 String
static std::unordered_map<AstKind, std::string> kindsInfo{
#define AST_INFO(KIND, STR, DEF) {AstKind::KIND, STR},
#include "visitor/AstInfo.inc"
#undef AST_INFO
};

MutAstVisitor::MutAstVisitor()
{
// 定义注册代码片段
#define GEN_REG_HANDLER(KIND, N)                                                                                       \
    registerHandler(                                                                                                   \
        AstKind::KIND, [this](AstNode& node) { return this->Before(Cangjie::StaticCast<N&>(node)); },                  \
        [this](AstNode& node, VisitResult& res) { this->Visit(Cangjie::StaticCast<N&>(node), res); },                  \
        [this](AstNode& node, const VisitResult& res) { this->After(Cangjie::StaticCast<N&>(node), res); })
// 使用宏生成代码
#define AST_INFO(KIND, STR, DEF) GEN_REG_HANDLER(KIND, DEF);
#include "visitor/AstInfo.inc"
#undef AST_INFO
}

void MutAstVisitor::registerHandler(AstKind kind, BeforeFunc before, VisitFunc visit, AfterFunc after)
{
    handlers[kind] = {before, visit, after};
    Logger::Get().Debug("MutAstVisitor:registerHandler", "For ", kindsInfo[kind]);
}

VisitResult MutAstVisitor::BeforeVisit(AstNode& node)
{
    // Logger::Get().Debug("MutAstVisitor:BeforeVisit", "For ", static_cast<int>(node.astKind));
    auto it = handlers.find(node.astKind);
    if (it != handlers.end() && std::get<0>(it->second)) {
        return std::get<0>(it->second)(node);
    } else {
        return DefaultBefore(node);
    }
}

void MutAstVisitor::Visit(AstNode& node, VisitResult& res)
{
    auto it = handlers.find(node.astKind);
    if (it != handlers.end() && std::get<1>(it->second)) {
        return std::get<1>(it->second)(node, res);
    } else {
        return DefaultVisit(node, res);
    }
}

void MutAstVisitor::AfterVisit(AstNode& node, const VisitResult& res)
{
    // Logger::Get().Debug("MutAstVisitor:AfterVisit", "For ", static_cast<int>(node.astKind));
    auto it = handlers.find(node.astKind);
    if (it != handlers.end() && std::get<2>(it->second)) {
        return std::get<2>(it->second)(node, res);
    } else {
        return DefaultAfter(node, res);
    }
}

VisitResult MutAstVisitor::DefaultBefore(AstNode& node)
{
    return VisitResult::Cont();
}

void MutAstVisitor::DefaultVisit(AstNode& node, VisitResult& res)
{
}

void MutAstVisitor::DefaultAfter(AstNode& node, const VisitResult& res)
{
}

// Visit
// 定义默认实现宏
#define GEN_MUTVISIT_DEFAULT_IMPL(N)                                                                                   \
    void MutAstVisitor::Visit(N& node, VisitResult& res)                                                               \
    {                                                                                                                  \
    }
// 递归展开宏
EXPAND2(GEN_MUTVISIT_DEFAULT_IMPL, Modifier, Annotation);
EXPAND2(GEN_MUTVISIT_DEFAULT_IMPL, PrimitiveType, ThisType);
EXPAND4(GEN_MUTVISIT_DEFAULT_IMPL, WildcardPattern, WildcardExpr, PrimitiveTypeExpr, JumpExpr);
EXPAND1(GEN_MUTVISIT_DEFAULT_IMPL, MacroExpandParam); // To check

void MutAstVisitor::Visit(ImportContent& node, VisitResult& res)
{
    for (auto& ic : node.items) {
        MutTraverse(ic, *this);
    }
}

void MutAstVisitor::Visit(ImportSpec& node, VisitResult& res)
{
    VisitNode(node.modifier);
    Visit(node.content, res);
}

void MutAstVisitor::Visit(Package& node, VisitResult& res)
{
    VisitNodes(node.files);
}

void MutAstVisitor::Visit(PackageSpec& node, VisitResult& res)
{
    if (node.modifier) {
        MutTraverse(*node.modifier, *this);
    }
}

void MutAstVisitor::Visit(File& node, VisitResult& res)
{
    VisitNode(node.package);
    VisitNodes(node.imports);
    VisitNodes(node.decls);
}

void MutAstVisitor::Visit(FuncDecl& node, VisitResult& res)
{
    Logger::Get().Debug("MutAstVisitor:Visit", "For FuncDecl: ", node.identifier.Val());
    VisitDecl(node, res);
    VisitNode(node.funcBody);
}

void MutAstVisitor::Visit(FuncBody& node, VisitResult& res)
{
    AH_ASSERT(node.paramLists.size() == 1);
    VisitNode(node.paramLists[0]);
    VisitNode(node.generic);
    VisitNode(node.retType);
    VisitNode(node.body);
}

void MutAstVisitor::Visit(FuncParamList& node, VisitResult& res)
{
    VisitNodes(node.params);
}

void MutAstVisitor::Visit(FuncParam& node, VisitResult& res)
{
    VisitDecl(node, res);
    VisitNode(node.assignment);
}

void MutAstVisitor::Visit(VarDecl& node, VisitResult& res)
{
    VisitDecl(node, res);
    VisitNode(node.type);
}

void MutAstVisitor::Visit(ClassDecl& node, VisitResult& res)
{
    VisitDecl(node, res);
    VisitNodes(node.inheritedTypes);
    VisitNode(node.body);
}

void MutAstVisitor::Visit(ClassBody& node, VisitResult& res)
{
    VisitNodes(node.decls);
}

void MutAstVisitor::Visit(PrimaryCtorDecl& node, VisitResult& res)
{
    VisitDecl(node, res);
    VisitNode(node.funcBody);
}

void MutAstVisitor::Visit(StructDecl& node, VisitResult& res)
{
    VisitDecl(node, res);
    VisitNodes(node.inheritedTypes);
    VisitNode(node.body);
}

void MutAstVisitor::Visit(StructBody& node, VisitResult& res)
{
    VisitNodes(node.decls);
}

void MutAstVisitor::Visit(InterfaceDecl& node, VisitResult& res)
{
    VisitDecl(node, res);
    VisitNodes(node.inheritedTypes);
    VisitNode(node.body);
}

void MutAstVisitor::Visit(InterfaceBody& node, VisitResult& res)
{
    VisitNodes(node.decls);
}

void MutAstVisitor::Visit(EnumDecl& node, VisitResult& res)
{
    VisitDecl(node, res);
    VisitNodes(node.inheritedTypes);
    VisitNodes(node.constructors);
    VisitNodes(node.members);
}

void MutAstVisitor::Visit(ExtendDecl& node, VisitResult& res)
{
    VisitDecl(node, res);
    VisitNode(node.extendedType);
    VisitNodes(node.inheritedTypes);
    VisitNodes(node.members);
}

void MutAstVisitor::Visit(PropDecl& node, VisitResult& res)
{
    VisitDecl(node, res);
    VisitNodes(node.getters);
    VisitNodes(node.setters);
}

void MutAstVisitor::Visit(VarWithPatternDecl& node, VisitResult& res)
{
    VisitDecl(node, res);
    VisitNode(node.irrefutablePattern);
}

void MutAstVisitor::Visit(MainDecl& node, VisitResult& res)
{
    VisitDecl(node, res);
    VisitNode(node.funcBody);
    // Desugar Decl?
}

void MutAstVisitor::Visit(TypeAliasDecl& node, VisitResult& res)
{
    VisitDecl(node, res);
    VisitNode(node.type);
}

void MutAstVisitor::Visit(RefType& node, VisitResult& res)
{
    VisitNodes(node.typeArguments);
}

void MutAstVisitor::Visit(FuncType& node, VisitResult& res)
{
    VisitNodes(node.paramTypes);
    VisitNode(node.retType);
}

void MutAstVisitor::Visit(VArrayType& node, VisitResult& res)
{
    VisitNode(node.typeArgument);
    VisitNode(node.constantType);
}

void MutAstVisitor::Visit(ConstantType& node, VisitResult& res)
{
    VisitNode(node.constantExpr);
}

void MutAstVisitor::Visit(ParenType& node, VisitResult& res)
{
    VisitNode(node.type);
}

void MutAstVisitor::Visit(OptionType& node, VisitResult& res)
{
    VisitNode(node.componentType);
}

void MutAstVisitor::Visit(TupleType& node, VisitResult& res)
{
    VisitNodes(node.fieldTypes);
}

void MutAstVisitor::Visit(QualifiedType& node, VisitResult& res)
{
    VisitNode(node.baseType);
    VisitNodes(node.typeArguments);
}

// Patterns
void MutAstVisitor::Visit(ConstPattern& node, VisitResult& res)
{
    VisitNode(node.literal);
    VisitNode(node.operatorCallExpr);
}

void MutAstVisitor::Visit(TypePattern& node, VisitResult& res)
{
    VisitNode(node.pattern);
    VisitNode(node.type);
    // desugar?
    VisitNode(node.desugarExpr);
    VisitNode(node.desugarVarPattern);
}

void MutAstVisitor::Visit(VarPattern& node, VisitResult& res)
{
    VisitNode(node.varDecl);
    VisitNode(node.desugarExpr); // desugar?
}

void MutAstVisitor::Visit(TuplePattern& node, VisitResult& res)
{
    VisitNodes(node.patterns);
}

void MutAstVisitor::Visit(EnumPattern& node, VisitResult& res)
{
    VisitNode(node.constructor);
    VisitNodes(node.patterns);
}

void MutAstVisitor::Visit(VarOrEnumPattern& node, VisitResult& res)
{
    VisitNode(node.pattern);
}

void MutAstVisitor::Visit(ExceptTypePattern& node, VisitResult& res)
{
    VisitNode(node.pattern);
    VisitNodes(node.types);
}

void MutAstVisitor::Visit(Block& node, VisitResult& res)
{
    VisitNodes(node.body);
}

void MutAstVisitor::Visit(LitConstExpr& node, VisitResult& res)
{
    VisitNode(node.ref);
    VisitNode(node.siExpr);
}

void MutAstVisitor::Visit(ReturnExpr& node, VisitResult& res)
{
    VisitNode(node.expr);
}

void MutAstVisitor::Visit(PointerExpr& node, VisitResult& res)
{
    VisitNode(node.type);
    VisitNode(node.arg);
}

void MutAstVisitor::Visit(CallExpr& node, VisitResult& res)
{
    VisitNode(node.baseFunc);
    VisitNodes(node.args);
    VisitNodes(node.defaultArgs); // To Check this
}

void MutAstVisitor::Visit(FuncArg& node, VisitResult& res)
{
    VisitNode(node.expr);
}

void MutAstVisitor::Visit(MemberAccess& node, VisitResult& res)
{
    VisitNode(node.baseExpr);
    VisitNodes(node.typeArguments);
}

void MutAstVisitor::Visit(RefExpr& node, VisitResult& res)
{
    VisitNodes(node.typeArguments);
}

void MutAstVisitor::Visit(IfExpr& node, VisitResult& res)
{
    VisitNode(node.condExpr);
    VisitNode(node.thenBody);
    VisitNode(node.elseBody);
}

void MutAstVisitor::Visit(DoWhileExpr& node, VisitResult& res)
{
    VisitNode(node.body);
    VisitNode(node.condExpr);
}

void MutAstVisitor::Visit(WhileExpr& node, VisitResult& res)
{
    VisitNode(node.condExpr);
    VisitNode(node.body);
}

void MutAstVisitor::Visit(ForInExpr& node, VisitResult& res)
{
    VisitNode(node.pattern);
    VisitNode(node.patternGuard);
    VisitNode(node.inExpression);
    VisitNode(node.body);
}

void MutAstVisitor::Visit(LetPatternDestructor& node, VisitResult& res)
{
    VisitNodes(node.patterns);
    VisitNode(node.initializer);
}

void MutAstVisitor::Visit(OptionalExpr& node, VisitResult& res)
{
    VisitNode(node.baseExpr);
}

void MutAstVisitor::Visit(OptionalChainExpr& node, VisitResult& res)
{
    VisitNode(node.expr);
}

void MutAstVisitor::Visit(AssignExpr& node, VisitResult& res)
{
    VisitNode(node.leftValue);
    VisitNode(node.rightExpr);
}

void MutAstVisitor::Visit(IncOrDecExpr& node, VisitResult& res)
{
    VisitNode(node.expr);
}

void MutAstVisitor::Visit(UnaryExpr& node, VisitResult& res)
{
    VisitNode(node.expr);
}

void MutAstVisitor::Visit(BinaryExpr& node, VisitResult& res)
{
    VisitNode(node.leftExpr);
    VisitNode(node.rightExpr);
}

void MutAstVisitor::Visit(SubscriptExpr& node, VisitResult& res)
{
    VisitNode(node.baseExpr);
    VisitNodes(node.indexExprs);
}

void MutAstVisitor::Visit(TypeConvExpr& node, VisitResult& res)
{
    VisitNode(node.type);
    VisitNode(node.expr);
}

void MutAstVisitor::Visit(ParenExpr& node, VisitResult& res)
{
    VisitNode(node.expr);
}

void MutAstVisitor::Visit(IsExpr& node, VisitResult& res)
{
    VisitNode(node.leftExpr);
    VisitNode(node.isType);
}

void MutAstVisitor::Visit(AsExpr& node, VisitResult& res)
{
    VisitNode(node.leftExpr);
    VisitNode(node.asType);
}

void MutAstVisitor::Visit(RangeExpr& node, VisitResult& res)
{
    VisitNode(node.startExpr);
    VisitNode(node.stopExpr);
    VisitNode(node.stepExpr);
}

void MutAstVisitor::Visit(ArrayExpr& node, VisitResult& res)
{
    VisitNode(node.type);
    VisitNodes(node.args);
}

void MutAstVisitor::Visit(LambdaExpr& node, VisitResult& res)
{
    VisitNode(node.funcBody);
}

void MutAstVisitor::Visit(TrailingClosureExpr& node, VisitResult& res)
{
    VisitNode(node.expr);
    VisitNode(node.lambda);
}

void MutAstVisitor::Visit(TryExpr& node, VisitResult& res)
{
    VisitNodes(node.resourceSpec);
    VisitNode(node.tryBlock);
    VisitNodes(node.catchPatterns);
    VisitNodes(node.catchBlocks);
    VisitNode(node.finallyBlock);
}

void MutAstVisitor::Visit(ThrowExpr& node, VisitResult& res)
{
    VisitNode(node.expr);
}

void MutAstVisitor::Visit(MatchCase& node, VisitResult& res)
{
    VisitNodes(node.patterns);
    VisitNode(node.patternGuard);
    VisitNode(node.exprOrDecls);
}

void MutAstVisitor::Visit(MatchCaseOther& node, VisitResult& res)
{
    VisitNode(node.matchExpr);
    VisitNode(node.exprOrDecls);
}

void MutAstVisitor::Visit(MatchExpr& node, VisitResult& res)
{
    VisitNode(node.selector);
    VisitNodes(node.matchCases);
    VisitNodes(node.matchCaseOthers);
}

void MutAstVisitor::Visit(TupleLit& node, VisitResult& res)
{
    VisitNodes(node.children);
}

void MutAstVisitor::Visit(ArrayLit& node, VisitResult& res)
{
    VisitNodes(node.children);
}

void MutAstVisitor::Visit(SpawnExpr& node, VisitResult& res)
{
    VisitNode(node.futureObj);
    VisitNode(node.task);
    VisitNode(node.arg);
}

void MutAstVisitor::Visit(SynchronizedExpr& node, VisitResult& res)
{
    VisitNode(node.mutex);
    VisitNode(node.body);
}

// Generic
void MutAstVisitor::Visit(GenericConstraint& node, VisitResult& res)
{
    VisitNode(node.type);
    VisitNodes(node.upperBounds);
}

void MutAstVisitor::Visit(Generic& node, VisitResult& res)
{
    VisitNodes(node.typeParameters);
    VisitNodes(node.genericConstraints);
}

void MutAstVisitor::Visit(GenericParamDecl& node, VisitResult& res)
{
    VisitDecl(node, res);
}

// Macro
void MutAstVisitor::Visit(MacroDecl& node, VisitResult& res)
{
    VisitDecl(node, res);
    VisitNode(node.funcBody);
    // Desugared Func?
}

void MutAstVisitor::Visit(MacroExpandDecl& node, VisitResult& res)
{
    // To Check
    VisitDecl(node, res);
    VisitNode(node.invocation.decl);
    VisitNodes(node.invocation.nodes);
}

void MutAstVisitor::Visit(TokenPart& node, VisitResult& res)
{
}

void MutAstVisitor::Visit(QuoteExpr& node, VisitResult& res)
{
    VisitNodes(node.exprs);
}

void MutAstVisitor::Visit(MacroExpandExpr& node, VisitResult& res)
{
    VisitNodes(node.annotations);
    for (auto mod : node.modifiers) {
        Visit(mod, res);
    }
}

void MutAstVisitor::Visit(InterpolationExpr& node, VisitResult& res)
{
    VisitNode(node.block);
}

void MutAstVisitor::Visit(StrInterpolationExpr& node, VisitResult& res)
{
    VisitNodes(node.strPartExprs);
}

void MutAstVisitor::Visit(BuiltInDecl& node, VisitResult& res)
{
}

void MutAstVisitor::VisitDecl(Decl& node, VisitResult& res)
{
    VisitNodes(node.annotations);
    VisitNode(node.annotationsArray);
    // std::set<Modifier> 无法获取可变引用，只能构造新的
    // auto& ms = node.modifiers;
    // for (Modifier& mod : ms) {
    //     Visit(mod, res);
    // }
    VisitNode(node.generic);
}
