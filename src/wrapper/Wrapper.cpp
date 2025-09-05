/**
 * @file
 *
 * This file implements the wrapper ast nodes.
 */

#include "utils/Cast.h"
#include "utils/Logger.h"
#include "utils/Macro.h"
#include "wrapper/WrapperAst.h"

std::string AstKind2Str(AstKind kind)
{
    static std::unordered_map<AstKind, std::string> kindsInfo{
#define AST_INFO(KIND, STR, DEF) {AstKind::KIND, STR},
#include "wrapper/AstInfo.inc"
#undef AST_INFO
    };
    return kindsInfo.at(kind);
}

std::vector<Ptr<AstNode>> AstNodeHelper::GetChildren(const AstNode& node)
{
    std::vector<Ptr<AstNode>> result;
    if (auto fn = AstNodeHelper::GetInstance().handlers.TryGet<CollectFunc>(node.astKind)) {
        fn->get()(node, result);
    } else {
        Logger::Get().Warn("AstNodeHelper::GetChildren", "unregistered kind ", AstKind2Str(node.astKind));
    }
    return std::move(result);
}

void AstNodeHelper::ReplaceChildren(AstNode& node, std::vector<OwnedPtr<AstNode>>& children)
{
    if (auto fn = AstNodeHelper::GetInstance().handlers.TryGet<ReplaceFunc>(node.astKind)) {
        fn->get()(node, children);
    } else {
        Logger::Get().Warn("AstNodeHelper::ReplaceChildren", "unregistered kind ", AstKind2Str(node.astKind));
    }
}

std::unique_ptr<AstNodeHelper> AstNodeHelper::helper;

AstNodeHelper& AstNodeHelper::GetInstance()
{
    if (!helper) {
        helper = std::unique_ptr<AstNodeHelper>(new AstNodeHelper());
    }
    return *helper;
}

AstNodeHelper::AstNodeHelper()
{
    RegCollectHandlers();
    RegReplaceHandlers();
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
} // namespace

void AstNodeHelper::RegCollectHandlers()
{
    // No Childrend Node: Modifier, Annotation, PrimitiveType, ThisType, WildcardPattern, WildcardExpr,
    // PrimitiveTypeExpr
    // JumpExpr
    handlers
        .Reg<CollectFunc>(AstKind::FILE,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& file = Cast<const File&>(node);
                CollectChildren(file.package, children);
                CollectChildren(file.imports, children);
                CollectChildren(file.decls, children);
            })
        .Reg<CollectFunc>(AstKind::PACKAGE,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                CollectChildren(Cast<const Package&>(node).files, children);
            })
        .Reg<CollectFunc>(AstKind::PACKAGE_SPEC,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                CollectChildren(Cast<const PackageSpec&>(node).modifier, children);
            })
        .Reg<CollectFunc>(AstKind::IMPORT_SPEC,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& is = Cast<const ImportSpec&>(node);
                CollectChildren(is.modifier, children);
                // children.push_back(&is.content);
            })
        .Reg<CollectFunc>(AstKind::INTERFACE_BODY,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                CollectChildren(Cast<const InterfaceBody&>(node).decls, children);
            })
        .Reg<CollectFunc>(AstKind::CLASS_BODY,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                CollectChildren(Cast<const ClassBody&>(node).decls, children);
            })
        .Reg<CollectFunc>(AstKind::STRUCT_BODY,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                CollectChildren(Cast<const StructBody&>(node).decls, children);
            })
        .Reg<CollectFunc>(AstKind::FUNC_BODY,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& body = Cast<const FuncBody&>(node);
                CollectChildren(body.paramLists[0], children);
                CollectChildren(body.generic, children);
                CollectChildren(body.retType, children);
                CollectChildren(body.body, children);
            })
        .Reg<CollectFunc>(AstKind::FUNC_PARAM_LIST,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                CollectChildren(Cast<const FuncParamList&>(node).params, children);
            })
        .Reg<CollectFunc>(AstKind::FUNC_ARG,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                CollectChildren(Cast<const FuncArg&>(node).expr, children);
            })
        .Reg<CollectFunc>(AstKind::MATCH_CASE_OTHER,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& mco = Cast<const MatchCaseOther&>(node);
                CollectChildren(mco.matchExpr, children);
                CollectChildren(mco.exprOrDecls, children);
            })
        .Reg<CollectFunc>(AstKind::MATCH_CASE,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& mc = Cast<const MatchCase&>(node);
                CollectChildren(mc.patterns, children);
                CollectChildren(mc.patternGuard, children);
                CollectChildren(mc.exprOrDecls, children);
            })
        .Reg<CollectFunc>(AstKind::GENERIC_CONSTRAINT,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& gc = Cast<const GenericConstraint&>(node);
                CollectChildren(gc.type, children);
                CollectChildren(gc.upperBounds, children);
            })
        .Reg<CollectFunc>(AstKind::GENERIC,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& generic = Cast<const Generic&>(node);
                CollectChildren(generic.typeParameters, children);
                CollectChildren(generic.genericConstraints, children);
            })
        .Reg<CollectFunc>(AstKind::MACRO_EXPAND_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const MacroExpandExpr&>(node);
                CollectChildren(expr.annotations, children);
                // To Check
                // for (const Modifier& mod : expr.modifiers) {
                //     children.push_back(&mod);
                // }
            })
        .Reg<CollectFunc>(AstKind::SYNCHRONIZED_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const SynchronizedExpr&>(node);
                CollectChildren(expr.mutex, children);
                CollectChildren(expr.body, children);
            })
        .Reg<CollectFunc>(AstKind::SPAWN_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const SpawnExpr&>(node);
                CollectChildren(expr.futureObj, children);
                CollectChildren(expr.task, children);
                CollectChildren(expr.arg, children);
            })
        .Reg<CollectFunc>(AstKind::THROW_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                CollectChildren(Cast<const ThrowExpr&>(node).expr, children);
            })
        .Reg<CollectFunc>(AstKind::TYPE_CONV_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const TypeConvExpr&>(node);
                CollectChildren(expr.type, children);
                CollectChildren(expr.expr, children);
            })
        .Reg<CollectFunc>(AstKind::DO_WHILE_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const DoWhileExpr&>(node);
                CollectChildren(expr.body, children);
                CollectChildren(expr.condExpr, children);
            })
        .Reg<CollectFunc>(AstKind::FOR_IN_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const ForInExpr&>(node);
                CollectChildren(expr.pattern, children);
                CollectChildren(expr.patternGuard, children);
                CollectChildren(expr.inExpression, children);
                CollectChildren(expr.body, children);
            })
        .Reg<CollectFunc>(AstKind::TRAIL_CLOSURE_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const TrailingClosureExpr&>(node);
                CollectChildren(expr.expr, children);
                CollectChildren(expr.lambda, children);
            })
        .Reg<CollectFunc>(AstKind::LAMBDA_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const LambdaExpr&>(node);
                CollectChildren(expr.funcBody, children);
            })
        .Reg<CollectFunc>(AstKind::WHILE_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const WhileExpr&>(node);
                CollectChildren(expr.condExpr, children);
                CollectChildren(expr.body, children);
            })
        .Reg<CollectFunc>(AstKind::TRY_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const TryExpr&>(node);
                CollectChildren(expr.resourceSpec, children);
                CollectChildren(expr.tryBlock, children);
                CollectChildren(expr.catchPatterns, children);
                CollectChildren(expr.catchBlocks, children);
                CollectChildren(expr.finallyBlock, children);
            })
        .Reg<CollectFunc>(AstKind::QUOTE_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const QuoteExpr&>(node);
                CollectChildren(expr.exprs, children);
            })
        .Reg<CollectFunc>(AstKind::LET_PATTERN_DESTRUCTOR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const LetPatternDestructor&>(node);
                CollectChildren(expr.patterns, children);
                CollectChildren(expr.initializer, children);
            })
        .Reg<CollectFunc>(AstKind::IF_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const IfExpr&>(node);
                CollectChildren(expr.condExpr, children);
                CollectChildren(expr.thenBody, children);
                CollectChildren(expr.elseBody, children);
            })
        .Reg<CollectFunc>(AstKind::BLOCK,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const Block&>(node);
                CollectChildren(expr.body, children);
            })
        .Reg<CollectFunc>(AstKind::MATCH_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const MatchExpr&>(node);
                CollectChildren(expr.selector, children);
                CollectChildren(expr.matchCases, children);
                CollectChildren(expr.matchCaseOthers, children);
            })
        .Reg<CollectFunc>(AstKind::TUPLE_LIT,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const TupleLit&>(node);
                CollectChildren(expr.children, children);
            })
        .Reg<CollectFunc>(AstKind::POINTER_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const PointerExpr&>(node);
                CollectChildren(expr.type, children);
                CollectChildren(expr.arg, children);
            })
        .Reg<CollectFunc>(AstKind::ARRAY_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const ArrayExpr&>(node);
                CollectChildren(expr.type, children);
                CollectChildren(expr.args, children);
            })
        .Reg<CollectFunc>(AstKind::ARRAY_LIT,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const ArrayLit&>(node);
                CollectChildren(expr.children, children);
            })
        .Reg<CollectFunc>(AstKind::RANGE_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const RangeExpr&>(node);
                CollectChildren(expr.startExpr, children);
                CollectChildren(expr.stopExpr, children);
                CollectChildren(expr.stepExpr, children);
            })
        .Reg<CollectFunc>(AstKind::AS_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const AsExpr&>(node);
                CollectChildren(expr.leftExpr, children);
                CollectChildren(expr.asType, children);
            })
        .Reg<CollectFunc>(AstKind::IS_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const IsExpr&>(node);
                CollectChildren(expr.leftExpr, children);
                CollectChildren(expr.isType, children);
            })
        .Reg<CollectFunc>(AstKind::SUBSCRIPT_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const SubscriptExpr&>(node);
                if (expr.desugarExpr) {
                    CollectChildren(expr.desugarExpr, children);
                } else {
                    CollectChildren(expr.baseExpr, children);
                    CollectChildren(expr.indexExprs, children);
                }
            })
        .Reg<CollectFunc>(AstKind::INC_OR_DEC_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const IncOrDecExpr&>(node);
                CollectChildren(expr.expr, children);
            })
        .Reg<CollectFunc>(AstKind::BINARY_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const BinaryExpr&>(node);
                CollectChildren(expr.leftExpr, children);
                CollectChildren(expr.rightExpr, children);
            })
        .Reg<CollectFunc>(AstKind::UNARY_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const UnaryExpr&>(node);
                CollectChildren(expr.expr, children);
            })
        .Reg<CollectFunc>(AstKind::ASSIGN_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const AssignExpr&>(node);
                CollectChildren(expr.leftValue, children);
                CollectChildren(expr.rightExpr, children);
            })

        .Reg<CollectFunc>(AstKind::STR_INTERPOLATION_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const StrInterpolationExpr&>(node);
                CollectChildren(expr.strPartExprs, children);
            })
        .Reg<CollectFunc>(AstKind::INTERPOLATION_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const InterpolationExpr&>(node);
                CollectChildren(expr.block, children);
            })
        .Reg<CollectFunc>(AstKind::LIT_CONST_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const LitConstExpr&>(node);
                CollectChildren(expr.ref, children);
                CollectChildren(expr.siExpr, children);
            })
        .Reg<CollectFunc>(AstKind::RETURN_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const ReturnExpr&>(node);
                CollectChildren(expr.expr, children);
            })
        .Reg<CollectFunc>(AstKind::OPTIONAL_CHAIN_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const OptionalChainExpr&>(node);
                CollectChildren(expr.expr, children);
            })
        .Reg<CollectFunc>(AstKind::OPTIONAL_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const OptionalExpr&>(node);
                CollectChildren(expr.baseExpr, children);
            })
        .Reg<CollectFunc>(AstKind::REF_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const RefExpr&>(node);
                CollectChildren(expr.typeArguments, children);
            })
        .Reg<CollectFunc>(AstKind::MEMBER_ACCESS,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const MemberAccess&>(node);
                CollectChildren(expr.baseExpr, children);
                CollectChildren(expr.typeArguments, children);
            })
        .Reg<CollectFunc>(AstKind::PAREN_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const ParenExpr&>(node);
                CollectChildren(expr.expr, children);
            })
        .Reg<CollectFunc>(AstKind::CALL_EXPR,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& expr = Cast<const CallExpr&>(node);
                CollectChildren(expr.baseFunc, children);
                CollectChildren(expr.args, children);
                CollectChildren(expr.defaultArgs, children);
            })
        .Reg<CollectFunc>(AstKind::TUPLE_TYPE,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& ty = Cast<const TupleType&>(node);
                CollectChildren(ty.fieldTypes, children);
            })
        .Reg<CollectFunc>(AstKind::FUNC_TYPE,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& ty = Cast<const FuncType&>(node);
                CollectChildren(ty.paramTypes, children);
                CollectChildren(ty.retType, children);
            })
        .Reg<CollectFunc>(AstKind::PAREN_TYPE,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& ty = Cast<const ParenType&>(node);
                CollectChildren(ty.type, children);
            })
        .Reg<CollectFunc>(AstKind::VARRAY_TYPE,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& ty = Cast<const VArrayType&>(node);
                CollectChildren(ty.typeArgument, children);
                CollectChildren(ty.constantType, children);
            })
        .Reg<CollectFunc>(AstKind::CONSTANT_TYPE,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& ty = Cast<const ConstantType&>(node);
                CollectChildren(ty.constantExpr, children);
            })
        .Reg<CollectFunc>(AstKind::OPTION_TYPE,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& ty = Cast<const OptionType&>(node);
                CollectChildren(ty.componentType, children);
            })
        .Reg<CollectFunc>(AstKind::QUALIFIED_TYPE,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& ty = Cast<const QualifiedType&>(node);
                CollectChildren(ty.baseType, children);
                CollectChildren(ty.typeArguments, children);
            })
        .Reg<CollectFunc>(AstKind::REF_TYPE,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& ty = Cast<const RefType&>(node);
                CollectChildren(ty.typeArguments, children);
            })
        .Reg<CollectFunc>(AstKind::EXCEPT_TYPE_PATTERN,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& pat = Cast<const ExceptTypePattern&>(node);
                CollectChildren(pat.pattern, children);
                CollectChildren(pat.types, children);
            })
        .Reg<CollectFunc>(AstKind::TYPE_PATTERN,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& pat = Cast<const TypePattern&>(node);
                CollectChildren(pat.pattern, children);
                CollectChildren(pat.type, children);
                // CollectChildren(pat.desugarExpr, children);
                // CollectChildren(pat.desugarVarPattern, children);
            })
        .Reg<CollectFunc>(AstKind::VAR_OR_ENUM_PATTERN,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& pat = Cast<const VarOrEnumPattern&>(node);
                CollectChildren(pat.pattern, children);
            })
        .Reg<CollectFunc>(AstKind::ENUM_PATTERN,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& pat = Cast<const EnumPattern&>(node);
                CollectChildren(pat.constructor, children);
                CollectChildren(pat.patterns, children);
            })
        .Reg<CollectFunc>(AstKind::TUPLE_PATTERN,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& pat = Cast<const TuplePattern&>(node);
                CollectChildren(pat.patterns, children);
            })
        .Reg<CollectFunc>(AstKind::CONST_PATTERN,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& pat = Cast<const ConstPattern&>(node);
                CollectChildren(pat.literal, children);
                CollectChildren(pat.operatorCallExpr, children);
            })
        .Reg<CollectFunc>(AstKind::VAR_PATTERN,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& ident = Cast<const VarPattern&>(node);
                CollectChildren(ident.varDecl, children);
                // CollectChildren(ident.desugarExpr, children);
            })
        .Reg<CollectFunc>(AstKind::MACRO_EXPAND_DECL,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& decl = Cast<const MacroExpandDecl&>(node);
                CollectChildren(decl, children);
                CollectChildren(decl.invocation.decl, children);
                CollectChildren(decl.invocation.nodes, children);
            })
        .Reg<CollectFunc>(AstKind::GENERIC_PARAM_DECL,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                CollectChildren(Cast<const GenericParamDecl&>(node), children);
            })
        .Reg<CollectFunc>(AstKind::VAR_WITH_PATTERN_DECL,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& decl = Cast<const VarWithPatternDecl&>(node);
                CollectChildren(decl, children);
                CollectChildren(decl.irrefutablePattern, children);
            })
        .Reg<CollectFunc>(AstKind::FUNC_PARAM,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& decl = Cast<const FuncParam&>(node);
                CollectChildren(decl, children);
                CollectChildren(decl.assignment, children);
            })
        .Reg<CollectFunc>(AstKind::MACRO_EXPAND_PARAM,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                CollectChildren(Cast<const MacroExpandParam&>(node), children);
            })
        .Reg<CollectFunc>(AstKind::PROP_DECL,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& decl = Cast<const PropDecl&>(node);
                CollectChildren(decl, children);
                CollectChildren(decl.getters, children);
                CollectChildren(decl.setters, children);
            })
        .Reg<CollectFunc>(AstKind::VAR_DECL,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& decl = Cast<const VarDecl&>(node);
                CollectChildren(decl, children);
                CollectChildren(decl.type, children);
                CollectChildren(decl.initializer, children);
            })
        .Reg<CollectFunc>(AstKind::BUILTIN_DECL,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                CollectChildren(Cast<const BuiltInDecl&>(node), children);
            })
        .Reg<CollectFunc>(AstKind::PRIMARY_CTOR_DECL,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& decl = Cast<const PrimaryCtorDecl&>(node);
                CollectChildren(decl, children);
                CollectChildren(decl.funcBody, children);
            })
        .Reg<CollectFunc>(AstKind::TYPE_ALIAS_DECL,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& decl = Cast<const TypeAliasDecl&>(node);
                CollectChildren(decl, children);
                CollectChildren(decl.type, children);
            })
        .Reg<CollectFunc>(AstKind::STRUCT_DECL,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& decl = Cast<const StructDecl&>(node);
                CollectChildren(decl, children);
                CollectChildren(decl.inheritedTypes, children);
                CollectChildren(decl.body, children);
            })
        .Reg<CollectFunc>(AstKind::ENUM_DECL,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& decl = Cast<const EnumDecl&>(node);
                CollectChildren(decl, children);
                CollectChildren(decl.inheritedTypes, children);
                CollectChildren(decl.constructors, children);
                CollectChildren(decl.members, children);
            })
        .Reg<CollectFunc>(AstKind::EXTEND_DECL,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& decl = Cast<const ExtendDecl&>(node);
                CollectChildren(decl, children);
                CollectChildren(decl.extendedType, children);
                CollectChildren(decl.inheritedTypes, children);
                CollectChildren(decl.members, children);
            })
        .Reg<CollectFunc>(AstKind::INTERFACE_DECL,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& decl = Cast<const InterfaceDecl&>(node);
                CollectChildren(decl, children);
                CollectChildren(decl.inheritedTypes, children);
                CollectChildren(decl.body, children);
            })
        .Reg<CollectFunc>(AstKind::CLASS_DECL,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& decl = Cast<const ClassDecl&>(node);
                CollectChildren(decl, children);
                CollectChildren(decl.inheritedTypes, children);
                CollectChildren(decl.body, children);
            })
        .Reg<CollectFunc>(AstKind::MACRO_DECL,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& decl = Cast<const MacroDecl&>(node);
                CollectChildren(decl, children);
                CollectChildren(decl.funcBody, children);
            })
        .Reg<CollectFunc>(AstKind::FUNC_DECL,
            [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
                auto& decl = Cast<const FuncDecl&>(node);
                CollectChildren(decl, children);
                CollectChildren(decl.funcBody, children);
            })
        .Reg<CollectFunc>(AstKind::MAIN_DECL, [](const AstNode& node, std::vector<Ptr<AstNode>>& children) {
            auto& decl = Cast<const MainDecl&>(node);
            CollectChildren(decl, children);
            CollectChildren(decl.funcBody, children);
        });
}

namespace {
template <typename T> OwnedPtr<T> OwnedCast(OwnedPtr<AstNode>&& base)
{
    if (base == nullptr) {
        return nullptr;
    }
    T* nodePtr = dynamic_cast<T*>(base.get().get());
    if (nodePtr == nullptr) {
        return nullptr; // 转换失败，原指针仍由传入的 unique_ptr 管理
    }
    return OwnedPtr<T>(static_cast<T*>(base.release()));
}

/**
 * @brief 尝试交换两个AstNode指针的所有权
 * @param dst 目标节点指针，将被替换为src的内容
 * @param src 源节点指针，将被替换为dst的内容
 * @note 仅当src和dst都非空且src可以转换为T类型时才执行交换
 */
template <std::derived_from<AstNode> T> inline void TrySwap(OwnedPtr<T>& dst, OwnedPtr<AstNode>& src)
{
    if (src == nullptr || dst == nullptr) {
        return;
    }
    if (T* ptr = dynamic_cast<T*>(src.get().get()); !ptr) {
        return;
    }
    T* tmp = static_cast<T*>(src.release());
    src.reset(dst.release());
    dst.reset(tmp);
}

template <typename T> using OwnedVec = std::vector<OwnedPtr<T>>;
using OwnedNodeIter = OwnedVec<AstNode>::iterator;

template <std::derived_from<AstNode> T>
inline void ReplaceRange(OwnedVec<T>& dst, OwnedNodeIter& begin, const OwnedNodeIter& end)
{
    for (auto i = 0; i < dst.size() && begin != end; i++, begin++) {
        TrySwap(dst[i], *begin);
    }
}

template <std::derived_from<AstNode> T> inline void ReplaceNode(OwnedPtr<T>& dst, OwnedNodeIter& pos)
{
    TrySwap(dst, *pos);
    pos++;
}
} // namespace

void AstNodeHelper::RegReplaceHandlers()
{
    handlers
        .Reg<ReplaceFunc>(AstKind::FILE,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& file = Cast<File&>(node);
                AH_ASSERT(children.size() == file.package ? 1 : 0 + file.imports.size() + file.decls.size());
                auto it = children.begin();
                ReplaceNode(file.package, it);
                ReplaceRange(file.imports, it, it + file.imports.size());
                ReplaceRange(file.decls, it, it + file.decls.size());
            })
        .Reg<ReplaceFunc>(AstKind::FUNC_DECL, [](AstNode& node, OwnedVec<AstNode>& children) {
            auto& fn = Cast<FuncDecl&>(node);
            auto it = children.begin();
            ReplaceRange(fn.annotations, it, it + fn.annotations.size());
            ReplaceNode(fn.annotationsArray, it);
            ReplaceNode(fn.generic, it);
            ReplaceNode(fn.funcBody, it);
        });
}