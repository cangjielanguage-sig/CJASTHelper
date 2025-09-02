/**
 * @file
 *
 * This file implementation of AstVisitorBase.
 */

#include "utils/Cast.h"
#include "utils/Logger.h"
#include "visitor/AstVisitorBase.h"
#include "visitor/MutAstVisitorBase.h"

// VisitResult 实现方法
VisitResult::VisitResult(bool cont) : status(cont)
{
}

VisitResult VisitResult::Cont()
{
    return {true};
}

VisitResult VisitResult::Skip()
{
    return {false};
}

// Traverse 实现方法
VisitResult Traverse(const AstNode& node, AstVisitorBase& visitor)
{
    auto res = visitor.BeforeVisit(node);
    if (!res.status) {
        return res;
    }
    visitor.Visit(node, res);
    if (res.status) {
        visitor.AfterVisit(node, res);
    }
    return res;
}

// MutTraverse 实现方法
VisitResult MutTraverse(AstNode& node, MutAstVisitorBase& visitor)
{
    VisitResult res = visitor.BeforeVisit(node);
    if (!res.status) {
        return res;
    }
    res = visitor.Visit(node, res);
    if (res.status) {
        res = visitor.AfterVisit(node, res);
    }
    return res;
}

namespace {
template <typename T> inline void CollectChildren(const OwnedPtr<T>& node, std::vector<Ptr<AstNode>>& children)
{
    if (node) {
        children.push_back(node);
    }
}
template <typename T>
inline void CollectChildren(const std::vector<OwnedPtr<T>>& nodes, std::vector<Ptr<AstNode>>& children)
{
    for (auto& node : nodes) {
        CollectChildren(node, children);
    }
}

inline void CollectChildren(const Decl& decl, std::vector<Ptr<AstNode>>& children)
{
    CollectChildren(decl.annotations, children);
    CollectChildren(decl.annotationsArray, children);
    // TODO: Modifiers
    CollectChildren(decl.generic, children);
}

std::unordered_map<AstKind, std::function<void(const AstNode&, std::vector<Ptr<AstNode>>&)>> getChildrenMap = {
    {AstKind::FILE,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& file = Cast<const File&>(node);
            CollectChildren(file.package, children);
            CollectChildren(file.imports, children);
            CollectChildren(file.decls, children);
        }},
    {AstKind::PACKAGE,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            CollectChildren(Cast<const Package&>(node).files, children);
        }},
    {AstKind::PACKAGE_SPEC,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            CollectChildren(Cast<const PackageSpec&>(node).modifier, children);
        }},
    {AstKind::IMPORT_SPEC,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& is = Cast<const ImportSpec&>(node);
            CollectChildren(is.modifier, children);
            // children.push_back(&is.content);
        }},
    {AstKind::IMPORT_CONTENT,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            // auto& ic = Cast<const ImportContent&>(node);
            // for (auto& item : ic.items) {
            //     children.push_back(&item);
            // }
        }},
    {AstKind::INTERFACE_BODY,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            CollectChildren(Cast<const InterfaceBody&>(node).decls, children);
        }},
    {AstKind::CLASS_BODY,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            CollectChildren(Cast<const ClassBody&>(node).decls, children);
        }},
    {AstKind::STRUCT_BODY,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            CollectChildren(Cast<const StructBody&>(node).decls, children);
        }},
    {AstKind::FUNC_BODY,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& body = Cast<const FuncBody&>(node);
            CollectChildren(body.paramLists[0], children);
            CollectChildren(body.generic, children);
            CollectChildren(body.retType, children);
            CollectChildren(body.body, children);
        }},
    {AstKind::FUNC_PARAM_LIST,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            CollectChildren(Cast<const FuncParamList&>(node).params, children);
        }},
    {AstKind::FUNC_ARG,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            CollectChildren(Cast<const FuncArg&>(node).expr, children);
        }},
    {AstKind::MATCH_CASE_OTHER,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& mco = Cast<const MatchCaseOther&>(node);
            CollectChildren(mco.matchExpr, children);
            CollectChildren(mco.exprOrDecls, children);
        }},
    {AstKind::MATCH_CASE,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& mc = Cast<const MatchCase&>(node);
            CollectChildren(mc.patterns, children);
            CollectChildren(mc.patternGuard, children);
            CollectChildren(mc.exprOrDecls, children);
        }},
    {AstKind::GENERIC_CONSTRAINT,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& gc = Cast<const GenericConstraint&>(node);
            CollectChildren(gc.type, children);
            CollectChildren(gc.upperBounds, children);
        }},
    {AstKind::GENERIC,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& generic = Cast<const Generic&>(node);
            CollectChildren(generic.typeParameters, children);
            CollectChildren(generic.genericConstraints, children);
        }},
    {AstKind::MACRO_EXPAND_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const MacroExpandExpr&>(node);
            CollectChildren(expr.annotations, children);
            // To Check
            // for (const Modifier& mod : expr.modifiers) {
            //     children.push_back(&mod);
            // }
        }},
    {AstKind::SYNCHRONIZED_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const SynchronizedExpr&>(node);
            CollectChildren(expr.mutex, children);
            CollectChildren(expr.body, children);
        }},
    {AstKind::SPAWN_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const SpawnExpr&>(node);
            CollectChildren(expr.futureObj, children);
            CollectChildren(expr.task, children);
            CollectChildren(expr.arg, children);
        }},
    {AstKind::THROW_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            CollectChildren(Cast<const ThrowExpr&>(node).expr, children);
        }},
    {AstKind::TYPE_CONV_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const TypeConvExpr&>(node);
            CollectChildren(expr.type, children);
            CollectChildren(expr.expr, children);
        }},
    {AstKind::DO_WHILE_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const DoWhileExpr&>(node);
            CollectChildren(expr.body, children);
            CollectChildren(expr.condExpr, children);
        }},
    {AstKind::FOR_IN_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const ForInExpr&>(node);
            CollectChildren(expr.pattern, children);
            CollectChildren(expr.patternGuard, children);
            CollectChildren(expr.inExpression, children);
            CollectChildren(expr.body, children);
        }},
    {AstKind::TRAIL_CLOSURE_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const TrailingClosureExpr&>(node);
            CollectChildren(expr.expr, children);
            CollectChildren(expr.lambda, children);
        }},
    {AstKind::LAMBDA_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const LambdaExpr&>(node);
            CollectChildren(expr.funcBody, children);
        }},
    {AstKind::WHILE_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const WhileExpr&>(node);
            CollectChildren(expr.condExpr, children);
            CollectChildren(expr.body, children);
        }},
    {AstKind::TRY_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const TryExpr&>(node);
            CollectChildren(expr.resourceSpec, children);
            CollectChildren(expr.tryBlock, children);
            CollectChildren(expr.catchPatterns, children);
            CollectChildren(expr.catchBlocks, children);
            CollectChildren(expr.finallyBlock, children);
        }},
    {AstKind::QUOTE_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const QuoteExpr&>(node);
            CollectChildren(expr.exprs, children);
        }},
    {AstKind::LET_PATTERN_DESTRUCTOR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const LetPatternDestructor&>(node);
            CollectChildren(expr.patterns, children);
            CollectChildren(expr.initializer, children);
        }},
    {AstKind::IF_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const IfExpr&>(node);
            CollectChildren(expr.condExpr, children);
            CollectChildren(expr.thenBody, children);
            CollectChildren(expr.elseBody, children);
        }},
    {AstKind::BLOCK,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const Block&>(node);
            CollectChildren(expr.body, children);
        }},
    {AstKind::MATCH_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const MatchExpr&>(node);
            CollectChildren(expr.selector, children);
            CollectChildren(expr.matchCases, children);
            CollectChildren(expr.matchCaseOthers, children);
        }},
    {AstKind::TUPLE_LIT,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const TupleLit&>(node);
            CollectChildren(expr.children, children);
        }},
    {AstKind::POINTER_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const PointerExpr&>(node);
            CollectChildren(expr.type, children);
            CollectChildren(expr.arg, children);
        }},
    {AstKind::ARRAY_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const ArrayExpr&>(node);
            CollectChildren(expr.type, children);
            CollectChildren(expr.args, children);
        }},
    {AstKind::ARRAY_LIT,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const ArrayLit&>(node);
            CollectChildren(expr.children, children);
        }},
    {AstKind::RANGE_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const RangeExpr&>(node);
            CollectChildren(expr.startExpr, children);
            CollectChildren(expr.stopExpr, children);
            CollectChildren(expr.stepExpr, children);
        }},
    {AstKind::AS_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const AsExpr&>(node);
            CollectChildren(expr.leftExpr, children);
            CollectChildren(expr.asType, children);
        }},
    {AstKind::IS_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const IsExpr&>(node);
            CollectChildren(expr.leftExpr, children);
            CollectChildren(expr.isType, children);
        }},
    {AstKind::SUBSCRIPT_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const SubscriptExpr&>(node);
            if (expr.desugarExpr) {
                CollectChildren(expr.desugarExpr, children);
            } else {
                CollectChildren(expr.baseExpr, children);
                CollectChildren(expr.indexExprs, children);
            }
        }},
    {AstKind::INC_OR_DEC_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const IncOrDecExpr&>(node);
            CollectChildren(expr.expr, children);
        }},
    {AstKind::BINARY_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const BinaryExpr&>(node);
            CollectChildren(expr.leftExpr, children);
            CollectChildren(expr.rightExpr, children);
        }},
    {AstKind::UNARY_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const UnaryExpr&>(node);
            CollectChildren(expr.expr, children);
        }},
    {AstKind::ASSIGN_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const AssignExpr&>(node);
            CollectChildren(expr.leftValue, children);
            CollectChildren(expr.rightExpr, children);
        }},

    {AstKind::STR_INTERPOLATION_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const StrInterpolationExpr&>(node);
            CollectChildren(expr.strPartExprs, children);
        }},
    {AstKind::INTERPOLATION_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const InterpolationExpr&>(node);
            CollectChildren(expr.block, children);
        }},
    {AstKind::LIT_CONST_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const LitConstExpr&>(node);
            CollectChildren(expr.ref, children);
            CollectChildren(expr.siExpr, children);
        }},
    {AstKind::RETURN_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const ReturnExpr&>(node);
            CollectChildren(expr.expr, children);
        }},
    {AstKind::OPTIONAL_CHAIN_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const OptionalChainExpr&>(node);
            CollectChildren(expr.expr, children);
        }},
    {AstKind::OPTIONAL_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const OptionalExpr&>(node);
            CollectChildren(expr.baseExpr, children);
        }},
    {AstKind::REF_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const RefExpr&>(node);
            CollectChildren(expr.typeArguments, children);
        }},
    {AstKind::MEMBER_ACCESS,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const MemberAccess&>(node);
            CollectChildren(expr.baseExpr, children);
            CollectChildren(expr.typeArguments, children);
        }},
    {AstKind::PAREN_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const ParenExpr&>(node);
            CollectChildren(expr.expr, children);
        }},
    {AstKind::CALL_EXPR,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& expr = Cast<const CallExpr&>(node);
            CollectChildren(expr.baseFunc, children);
            CollectChildren(expr.args, children);
            CollectChildren(expr.defaultArgs, children);
        }},
    {AstKind::TUPLE_TYPE,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& ty = Cast<const TupleType&>(node);
            CollectChildren(ty.fieldTypes, children);
        }},
    {AstKind::FUNC_TYPE,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& ty = Cast<const FuncType&>(node);
            CollectChildren(ty.paramTypes, children);
            CollectChildren(ty.retType, children);
        }},
    {AstKind::PAREN_TYPE,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& ty = Cast<const ParenType&>(node);
            CollectChildren(ty.type, children);
        }},
    {AstKind::VARRAY_TYPE,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& ty = Cast<const VArrayType&>(node);
            CollectChildren(ty.typeArgument, children);
            CollectChildren(ty.constantType, children);
        }},
    {AstKind::CONSTANT_TYPE,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& ty = Cast<const ConstantType&>(node);
            CollectChildren(ty.constantExpr, children);
        }},
    {AstKind::OPTION_TYPE,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& ty = Cast<const OptionType&>(node);
            CollectChildren(ty.componentType, children);
        }},
    {AstKind::QUALIFIED_TYPE,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& ty = Cast<const QualifiedType&>(node);
            CollectChildren(ty.baseType, children);
            CollectChildren(ty.typeArguments, children);
        }},
    {AstKind::REF_TYPE,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& ty = Cast<const RefType&>(node);
            CollectChildren(ty.typeArguments, children);
        }},
    {AstKind::EXCEPT_TYPE_PATTERN,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& pat = Cast<const ExceptTypePattern&>(node);
            CollectChildren(pat.pattern, children);
            CollectChildren(pat.types, children);
        }},
    {AstKind::TYPE_PATTERN,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& pat = Cast<const TypePattern&>(node);
            CollectChildren(pat.pattern, children);
            CollectChildren(pat.type, children);
            // CollectChildren(pat.desugarExpr, children);
            // CollectChildren(pat.desugarVarPattern, children);
        }},
    {AstKind::VAR_OR_ENUM_PATTERN,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& pat = Cast<const VarOrEnumPattern&>(node);
            CollectChildren(pat.pattern, children);
        }},
    {AstKind::ENUM_PATTERN,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& pat = Cast<const EnumPattern&>(node);
            CollectChildren(pat.constructor, children);
            CollectChildren(pat.patterns, children);
        }},
    {AstKind::TUPLE_PATTERN,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& pat = Cast<const TuplePattern&>(node);
            CollectChildren(pat.patterns, children);
        }},
    {AstKind::CONST_PATTERN,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& pat = Cast<const ConstPattern&>(node);
            CollectChildren(pat.literal, children);
            CollectChildren(pat.operatorCallExpr, children);
        }},
    {AstKind::VAR_PATTERN,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& ident = Cast<const VarPattern&>(node);
            CollectChildren(ident.varDecl, children);
            // CollectChildren(ident.desugarExpr, children);
        }},
    {AstKind::MACRO_EXPAND_DECL,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& decl = Cast<const MacroExpandDecl&>(node);
            CollectChildren(decl, children);
            CollectChildren(decl.invocation.decl, children);
            CollectChildren(decl.invocation.nodes, children);
        }},
    {AstKind::GENERIC_PARAM_DECL,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            CollectChildren(Cast<const GenericParamDecl&>(node), children);
        }},
    {AstKind::VAR_WITH_PATTERN_DECL,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& decl = Cast<const VarWithPatternDecl&>(node);
            CollectChildren(decl, children);
            CollectChildren(decl.irrefutablePattern, children);
        }},
    {AstKind::FUNC_PARAM,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& decl = Cast<const FuncParam&>(node);
            CollectChildren(decl, children);
            CollectChildren(decl.assignment, children);
        }},
    {AstKind::MACRO_EXPAND_PARAM,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            CollectChildren(Cast<const MacroExpandParam&>(node), children);
        }},
    {AstKind::PROP_DECL,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& decl = Cast<const PropDecl&>(node);
            CollectChildren(decl, children);
            CollectChildren(decl.getters, children);
            CollectChildren(decl.setters, children);
        }},
    {AstKind::VAR_DECL,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& decl = Cast<const VarDecl&>(node);
            CollectChildren(decl, children);
            CollectChildren(decl.type, children);
            CollectChildren(decl.initializer, children);
        }},
    {AstKind::BUILTIN_DECL,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            CollectChildren(Cast<const BuiltInDecl&>(node), children);
        }},
    {AstKind::PRIMARY_CTOR_DECL,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& decl = Cast<const PrimaryCtorDecl&>(node);
            CollectChildren(decl, children);
            CollectChildren(decl.funcBody, children);
        }},
    {AstKind::TYPE_ALIAS_DECL,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& decl = Cast<const TypeAliasDecl&>(node);
            CollectChildren(decl, children);
            CollectChildren(decl.type, children);
        }},
    {AstKind::STRUCT_DECL,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& decl = Cast<const StructDecl&>(node);
            CollectChildren(decl, children);
            CollectChildren(decl.inheritedTypes, children);
            CollectChildren(decl.body, children);
        }},
    {AstKind::ENUM_DECL,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& decl = Cast<const EnumDecl&>(node);
            CollectChildren(decl, children);
            CollectChildren(decl.inheritedTypes, children);
            CollectChildren(decl.constructors, children);
            CollectChildren(decl.members, children);
        }},
    {AstKind::EXTEND_DECL,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& decl = Cast<const ExtendDecl&>(node);
            CollectChildren(decl, children);
            CollectChildren(decl.extendedType, children);
            CollectChildren(decl.inheritedTypes, children);
            CollectChildren(decl.members, children);
        }},
    {AstKind::INTERFACE_DECL,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& decl = Cast<const InterfaceDecl&>(node);
            CollectChildren(decl, children);
            CollectChildren(decl.inheritedTypes, children);
            CollectChildren(decl.body, children);
        }},
    {AstKind::CLASS_DECL,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& decl = Cast<const ClassDecl&>(node);
            CollectChildren(decl, children);
            CollectChildren(decl.inheritedTypes, children);
            CollectChildren(decl.body, children);
        }},
    {AstKind::MACRO_DECL,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& decl = Cast<const MacroDecl&>(node);
            CollectChildren(decl, children);
            CollectChildren(decl.funcBody, children);
        }},
    {AstKind::FUNC_DECL,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& decl = Cast<const FuncDecl&>(node);
            CollectChildren(decl, children);
            CollectChildren(decl.funcBody, children);
        }},
    {AstKind::MAIN_DECL,
        [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& decl = Cast<const MainDecl&>(node);
            CollectChildren(decl, children);
            CollectChildren(decl.funcBody, children);
        }},
};
// No Childrend Node: Modifier, Annotation, PrimitiveType, ThisType, WildcardPattern, WildcardExpr, PrimitiveTypeExpr
// JumpExpr
} // namespace
std::vector<Ptr<AstNode>> AstNodeVisitor::GetChildren(const AstNode& node)
{
    std::vector<Ptr<AstNode>> result;
    if (auto fn = getChildrenMap.find(node.astKind); fn != getChildrenMap.end()) {
        fn->second(node, result);
    } else {
        Logger::Get().Warn("AstNodeVisitor::GetChildren", "unregistered kind ", AstKind2Str(node.astKind));
    }
    return result;
}
