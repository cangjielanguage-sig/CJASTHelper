/**
 * @file
 *
 * This file implements the wrapper ast nodes.
 */

#include "cangjie/AST/PrintNode.h"
#include "utils/Cast.h"
#include "utils/Logger.h"
#include "utils/Macro.h"
#include "wrapper/AstNodeHelper.h"
#include "wrapper/CangjieFrontendHelper.h"
#include <fstream>

CangjieFrontendHelper::CangjieFrontendHelper(ConStrVec& args, ConStrMap<Str>& env) : mci(ParseArgs(args, env), diag)
{
    ci.globalOptions.executablePath = ci.frontendOptions.environment.cangjieHome.value_or(".") + "/bin/cjc";
}

bool CangjieFrontendHelper::Parse()
{
    return mci.PerformParse();
}

bool CangjieFrontendHelper::ConditionCompile()
{
    return mci.PerformConditionCompile();
}

bool CangjieFrontendHelper::DesugaredParse()
{
    for (auto& pkg : mci.GetSourcePackages()) {
        Cangjie::PerformDesugarBeforeTypeCheck(*pkg);
    }
    return true;
}

bool CangjieFrontendHelper::ImportPackage()
{
    return mci.PerformImportPackage();
}

bool CangjieFrontendHelper::MacroExpand()
{
    return mci.PerformMacroExpand();
}

bool CangjieFrontendHelper::Sema()
{
    return mci.PerformSema();
}

bool CangjieFrontendHelper::DesugaredSema()
{
    return mci.PerformDesugarAfterSema();
}

Str CangjieFrontendHelper::GetOutDir() const
{
    return ci.globalOptions.outputDir.value_or(".");
}

PkgPtrVec CangjieFrontendHelper::GetSourcePackages()
{
    return mci.GetSourcePackages();
}

PkgPtrVec CangjieFrontendHelper::GetImportedPackages()
{
    return mci.GetPackages();
}

CompilerInvocation& CangjieFrontendHelper::ParseArgs(ConStrVec& args, ConStrMap<Str>& env)
{
    ci.frontendOptions.ReadPathsFromEnvironmentVars(env);
    ci.ParseArgs(args);
    return ci;
}

/**
 * @brief 获取AstKind对应的字符串
 */
Str AstKind2Str(AstKind kind)
{
    static UnorderedMap<AstKind, Str> kindsInfo{
#define AST_INFO(KIND, STR, DEF) {AstKind::KIND, STR},
#include "wrapper/AstInfo.inc"
#undef AST_INFO
    };
    return kindsInfo.at(kind);
}

Vec<Ptr<AstNode>> AstNodeHelper::GetChildren(const AstNode& node)
{
    Vec<Ptr<AstNode>> result;
    if (auto fn = AstNodeHelper::GetInstance().handlers.TryGet<CollectFunc>(node.astKind)) {
        fn->get()(node, result);
    }
    return result;
}

void AstNodeHelper::ReplaceChildren(AstNode& node, Vec<OwnedPtr<AstNode>>& children)
{
    if (auto fn = AstNodeHelper::GetInstance().handlers.TryGet<ReplaceFunc>(node.astKind)) {
        fn->get()(node, children);
    }
}

void AstNodeHelper::DumpAst(const AstNode& node, const Str& path)
{
    std::ofstream file(path);
    if (!file.is_open()) {
        LOGE("open file failed: ", path);
        return;
    }
    LOGD("dump ast to: ", path);
    Cangjie::PrintNode(&node, 0, "", file);
    file.close();
}

UniquePtr<AstNodeHelper> AstNodeHelper::helper;

AstNodeHelper& AstNodeHelper::GetInstance()
{
    if (!helper) {
        helper = UniquePtr<AstNodeHelper>(new AstNodeHelper());
    }
    return *helper;
}

AstNodeHelper::AstNodeHelper()
{
    RegCollectHandlers();
    RegReplaceHandlers();
}

namespace {
template <typename T> inline void CollectChildren(const OwnedPtr<T>& node, Vec<Ptr<AstNode>>& children)
{
    if (node) {
        children.push_back(node);
    }
}
template <typename T> inline void CollectChildren(const Vec<OwnedPtr<T>>& nodes, Vec<Ptr<AstNode>>& children)
{
    for (auto& node : nodes) {
        CollectChildren(node, children);
    }
}

inline void CollectChildren(const Decl& decl, Vec<Ptr<AstNode>>& children)
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
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& file = Cast<const File&>(node);
                CollectChildren(file.package, children);
                CollectChildren(file.imports, children);
                CollectChildren(file.decls, children);
            })
        .Reg<CollectFunc>(AstKind::PACKAGE,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                CollectChildren(Cast<const Package&>(node).files, children);
            })
        .Reg<CollectFunc>(AstKind::PACKAGE_SPEC,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                CollectChildren(Cast<const PackageSpec&>(node).modifier, children);
            })
        .Reg<CollectFunc>(AstKind::IMPORT_SPEC,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& is = Cast<const ImportSpec&>(node);
                CollectChildren(is.modifier, children);
                // children.push_back(&is.content);
            })
        .Reg<CollectFunc>(AstKind::INTERFACE_BODY,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                CollectChildren(Cast<const InterfaceBody&>(node).decls, children);
            })
        .Reg<CollectFunc>(AstKind::CLASS_BODY,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                CollectChildren(Cast<const ClassBody&>(node).decls, children);
            })
        .Reg<CollectFunc>(AstKind::STRUCT_BODY,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                CollectChildren(Cast<const StructBody&>(node).decls, children);
            })
        .Reg<CollectFunc>(AstKind::FUNC_BODY,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& body = Cast<const FuncBody&>(node);
                CollectChildren(body.paramLists[0], children);
                CollectChildren(body.generic, children);
                CollectChildren(body.retType, children);
                CollectChildren(body.body, children);
            })
        .Reg<CollectFunc>(AstKind::FUNC_PARAM_LIST,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                CollectChildren(Cast<const FuncParamList&>(node).params, children);
            })
        .Reg<CollectFunc>(AstKind::FUNC_ARG,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                CollectChildren(Cast<const FuncArg&>(node).expr, children);
            })
        .Reg<CollectFunc>(AstKind::MATCH_CASE_OTHER,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& mco = Cast<const MatchCaseOther&>(node);
                CollectChildren(mco.matchExpr, children);
                CollectChildren(mco.exprOrDecls, children);
            })
        .Reg<CollectFunc>(AstKind::MATCH_CASE,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& mc = Cast<const MatchCase&>(node);
                CollectChildren(mc.patterns, children);
                CollectChildren(mc.patternGuard, children);
                CollectChildren(mc.exprOrDecls, children);
            })
        .Reg<CollectFunc>(AstKind::GENERIC_CONSTRAINT,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& gc = Cast<const GenericConstraint&>(node);
                CollectChildren(gc.type, children);
                CollectChildren(gc.upperBounds, children);
            })
        .Reg<CollectFunc>(AstKind::GENERIC,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& generic = Cast<const Generic&>(node);
                CollectChildren(generic.typeParameters, children);
                CollectChildren(generic.genericConstraints, children);
            })
        .Reg<CollectFunc>(AstKind::MACRO_EXPAND_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const MacroExpandExpr&>(node);
                CollectChildren(expr.annotations, children);
                // To Check
                // for (const Modifier& mod : expr.modifiers) {
                //     children.push_back(&mod);
                // }
            })
        .Reg<CollectFunc>(AstKind::SYNCHRONIZED_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const SynchronizedExpr&>(node);
                CollectChildren(expr.mutex, children);
                CollectChildren(expr.body, children);
            })
        .Reg<CollectFunc>(AstKind::SPAWN_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const SpawnExpr&>(node);
                CollectChildren(expr.futureObj, children);
                CollectChildren(expr.task, children);
                CollectChildren(expr.arg, children);
            })
        .Reg<CollectFunc>(AstKind::THROW_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                CollectChildren(Cast<const ThrowExpr&>(node).expr, children);
            })
        .Reg<CollectFunc>(AstKind::TYPE_CONV_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const TypeConvExpr&>(node);
                CollectChildren(expr.type, children);
                CollectChildren(expr.expr, children);
            })
        .Reg<CollectFunc>(AstKind::DO_WHILE_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const DoWhileExpr&>(node);
                CollectChildren(expr.body, children);
                CollectChildren(expr.condExpr, children);
            })
        .Reg<CollectFunc>(AstKind::FOR_IN_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const ForInExpr&>(node);
                CollectChildren(expr.pattern, children);
                CollectChildren(expr.patternGuard, children);
                CollectChildren(expr.inExpression, children);
                CollectChildren(expr.body, children);
            })
        .Reg<CollectFunc>(AstKind::TRAIL_CLOSURE_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const TrailingClosureExpr&>(node);
                CollectChildren(expr.expr, children);
                CollectChildren(expr.lambda, children);
            })
        .Reg<CollectFunc>(AstKind::LAMBDA_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const LambdaExpr&>(node);
                CollectChildren(expr.funcBody, children);
            })
        .Reg<CollectFunc>(AstKind::WHILE_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const WhileExpr&>(node);
                CollectChildren(expr.condExpr, children);
                CollectChildren(expr.body, children);
            })
        .Reg<CollectFunc>(AstKind::TRY_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const TryExpr&>(node);
                CollectChildren(expr.resourceSpec, children);
                CollectChildren(expr.tryBlock, children);
                CollectChildren(expr.catchPatterns, children);
                CollectChildren(expr.catchBlocks, children);
                CollectChildren(expr.finallyBlock, children);
            })
        .Reg<CollectFunc>(AstKind::QUOTE_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const QuoteExpr&>(node);
                CollectChildren(expr.exprs, children);
            })
        .Reg<CollectFunc>(AstKind::LET_PATTERN_DESTRUCTOR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const LetPatternDestructor&>(node);
                CollectChildren(expr.patterns, children);
                CollectChildren(expr.initializer, children);
            })
        .Reg<CollectFunc>(AstKind::IF_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const IfExpr&>(node);
                CollectChildren(expr.condExpr, children);
                CollectChildren(expr.thenBody, children);
                CollectChildren(expr.elseBody, children);
            })
        .Reg<CollectFunc>(AstKind::BLOCK,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const Block&>(node);
                CollectChildren(expr.body, children);
            })
        .Reg<CollectFunc>(AstKind::MATCH_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const MatchExpr&>(node);
                CollectChildren(expr.selector, children);
                CollectChildren(expr.matchCases, children);
                CollectChildren(expr.matchCaseOthers, children);
            })
        .Reg<CollectFunc>(AstKind::TUPLE_LIT,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const TupleLit&>(node);
                CollectChildren(expr.children, children);
            })
        .Reg<CollectFunc>(AstKind::POINTER_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const PointerExpr&>(node);
                CollectChildren(expr.type, children);
                CollectChildren(expr.arg, children);
            })
        .Reg<CollectFunc>(AstKind::ARRAY_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const ArrayExpr&>(node);
                CollectChildren(expr.type, children);
                CollectChildren(expr.args, children);
            })
        .Reg<CollectFunc>(AstKind::ARRAY_LIT,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const ArrayLit&>(node);
                CollectChildren(expr.children, children);
            })
        .Reg<CollectFunc>(AstKind::RANGE_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const RangeExpr&>(node);
                CollectChildren(expr.startExpr, children);
                CollectChildren(expr.stopExpr, children);
                CollectChildren(expr.stepExpr, children);
            })
        .Reg<CollectFunc>(AstKind::AS_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const AsExpr&>(node);
                CollectChildren(expr.leftExpr, children);
                CollectChildren(expr.asType, children);
            })
        .Reg<CollectFunc>(AstKind::IS_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const IsExpr&>(node);
                CollectChildren(expr.leftExpr, children);
                CollectChildren(expr.isType, children);
            })
        .Reg<CollectFunc>(AstKind::SUBSCRIPT_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const SubscriptExpr&>(node);
                if (expr.desugarExpr) {
                    CollectChildren(expr.desugarExpr, children);
                } else {
                    CollectChildren(expr.baseExpr, children);
                    CollectChildren(expr.indexExprs, children);
                }
            })
        .Reg<CollectFunc>(AstKind::INC_OR_DEC_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const IncOrDecExpr&>(node);
                CollectChildren(expr.expr, children);
            })
        .Reg<CollectFunc>(AstKind::BINARY_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const BinaryExpr&>(node);
                CollectChildren(expr.leftExpr, children);
                CollectChildren(expr.rightExpr, children);
            })
        .Reg<CollectFunc>(AstKind::UNARY_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const UnaryExpr&>(node);
                CollectChildren(expr.expr, children);
            })
        .Reg<CollectFunc>(AstKind::ASSIGN_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const AssignExpr&>(node);
                CollectChildren(expr.leftValue, children);
                CollectChildren(expr.rightExpr, children);
            })

        .Reg<CollectFunc>(AstKind::STR_INTERPOLATION_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const StrInterpolationExpr&>(node);
                CollectChildren(expr.strPartExprs, children);
            })
        .Reg<CollectFunc>(AstKind::INTERPOLATION_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const InterpolationExpr&>(node);
                CollectChildren(expr.block, children);
            })
        .Reg<CollectFunc>(AstKind::LIT_CONST_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const LitConstExpr&>(node);
                CollectChildren(expr.ref, children);
                CollectChildren(expr.siExpr, children);
            })
        .Reg<CollectFunc>(AstKind::RETURN_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const ReturnExpr&>(node);
                CollectChildren(expr.expr, children);
            })
        .Reg<CollectFunc>(AstKind::OPTIONAL_CHAIN_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const OptionalChainExpr&>(node);
                CollectChildren(expr.expr, children);
            })
        .Reg<CollectFunc>(AstKind::OPTIONAL_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const OptionalExpr&>(node);
                CollectChildren(expr.baseExpr, children);
            })
        .Reg<CollectFunc>(AstKind::REF_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const RefExpr&>(node);
                CollectChildren(expr.typeArguments, children);
            })
        .Reg<CollectFunc>(AstKind::MEMBER_ACCESS,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const MemberAccess&>(node);
                CollectChildren(expr.baseExpr, children);
                CollectChildren(expr.typeArguments, children);
            })
        .Reg<CollectFunc>(AstKind::PAREN_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const ParenExpr&>(node);
                CollectChildren(expr.expr, children);
            })
        .Reg<CollectFunc>(AstKind::CALL_EXPR,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& expr = Cast<const CallExpr&>(node);
                CollectChildren(expr.baseFunc, children);
                CollectChildren(expr.args, children);
                CollectChildren(expr.defaultArgs, children);
            })
        .Reg<CollectFunc>(AstKind::TUPLE_TYPE,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& ty = Cast<const TupleType&>(node);
                CollectChildren(ty.fieldTypes, children);
            })
        .Reg<CollectFunc>(AstKind::FUNC_TYPE,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& ty = Cast<const FuncType&>(node);
                CollectChildren(ty.paramTypes, children);
                CollectChildren(ty.retType, children);
            })
        .Reg<CollectFunc>(AstKind::PAREN_TYPE,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& ty = Cast<const ParenType&>(node);
                CollectChildren(ty.type, children);
            })
        .Reg<CollectFunc>(AstKind::VARRAY_TYPE,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& ty = Cast<const VArrayType&>(node);
                CollectChildren(ty.typeArgument, children);
                CollectChildren(ty.constantType, children);
            })
        .Reg<CollectFunc>(AstKind::CONSTANT_TYPE,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& ty = Cast<const ConstantType&>(node);
                CollectChildren(ty.constantExpr, children);
            })
        .Reg<CollectFunc>(AstKind::OPTION_TYPE,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& ty = Cast<const OptionType&>(node);
                CollectChildren(ty.componentType, children);
            })
        .Reg<CollectFunc>(AstKind::QUALIFIED_TYPE,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& ty = Cast<const QualifiedType&>(node);
                CollectChildren(ty.baseType, children);
                CollectChildren(ty.typeArguments, children);
            })
        .Reg<CollectFunc>(AstKind::REF_TYPE,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& ty = Cast<const RefType&>(node);
                CollectChildren(ty.typeArguments, children);
            })
        .Reg<CollectFunc>(AstKind::EXCEPT_TYPE_PATTERN,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& pat = Cast<const ExceptTypePattern&>(node);
                CollectChildren(pat.pattern, children);
                CollectChildren(pat.types, children);
            })
        .Reg<CollectFunc>(AstKind::TYPE_PATTERN,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& pat = Cast<const TypePattern&>(node);
                CollectChildren(pat.pattern, children);
                CollectChildren(pat.type, children);
                // CollectChildren(pat.desugarExpr, children);
                // CollectChildren(pat.desugarVarPattern, children);
            })
        .Reg<CollectFunc>(AstKind::VAR_OR_ENUM_PATTERN,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& pat = Cast<const VarOrEnumPattern&>(node);
                CollectChildren(pat.pattern, children);
            })
        .Reg<CollectFunc>(AstKind::ENUM_PATTERN,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& pat = Cast<const EnumPattern&>(node);
                CollectChildren(pat.constructor, children);
                CollectChildren(pat.patterns, children);
            })
        .Reg<CollectFunc>(AstKind::TUPLE_PATTERN,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& pat = Cast<const TuplePattern&>(node);
                CollectChildren(pat.patterns, children);
            })
        .Reg<CollectFunc>(AstKind::CONST_PATTERN,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& pat = Cast<const ConstPattern&>(node);
                CollectChildren(pat.literal, children);
                CollectChildren(pat.operatorCallExpr, children);
            })
        .Reg<CollectFunc>(AstKind::VAR_PATTERN,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& ident = Cast<const VarPattern&>(node);
                CollectChildren(ident.varDecl, children);
                // CollectChildren(ident.desugarExpr, children);
            })
        .Reg<CollectFunc>(AstKind::MACRO_EXPAND_DECL,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& decl = Cast<const MacroExpandDecl&>(node);
                CollectChildren(decl, children);
                CollectChildren(decl.invocation.decl, children);
                CollectChildren(decl.invocation.nodes, children);
            })
        .Reg<CollectFunc>(AstKind::GENERIC_PARAM_DECL,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                CollectChildren(Cast<const GenericParamDecl&>(node), children);
            })
        .Reg<CollectFunc>(AstKind::VAR_WITH_PATTERN_DECL,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& decl = Cast<const VarWithPatternDecl&>(node);
                CollectChildren(decl, children);
                CollectChildren(decl.irrefutablePattern, children);
            })
        .Reg<CollectFunc>(AstKind::FUNC_PARAM,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& decl = Cast<const FuncParam&>(node);
                CollectChildren(decl, children);
                CollectChildren(decl.assignment, children);
            })
        .Reg<CollectFunc>(AstKind::MACRO_EXPAND_PARAM,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                CollectChildren(Cast<const MacroExpandParam&>(node), children);
            })
        .Reg<CollectFunc>(AstKind::PROP_DECL,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& decl = Cast<const PropDecl&>(node);
                CollectChildren(decl, children);
                CollectChildren(decl.getters, children);
                CollectChildren(decl.setters, children);
            })
        .Reg<CollectFunc>(AstKind::VAR_DECL,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& decl = Cast<const VarDecl&>(node);
                CollectChildren(decl, children);
                CollectChildren(decl.type, children);
                CollectChildren(decl.initializer, children);
            })
        .Reg<CollectFunc>(AstKind::BUILTIN_DECL,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                CollectChildren(Cast<const BuiltInDecl&>(node), children);
            })
        .Reg<CollectFunc>(AstKind::PRIMARY_CTOR_DECL,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& decl = Cast<const PrimaryCtorDecl&>(node);
                CollectChildren(decl, children);
                CollectChildren(decl.funcBody, children);
            })
        .Reg<CollectFunc>(AstKind::TYPE_ALIAS_DECL,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& decl = Cast<const TypeAliasDecl&>(node);
                CollectChildren(decl, children);
                CollectChildren(decl.type, children);
            })
        .Reg<CollectFunc>(AstKind::STRUCT_DECL,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& decl = Cast<const StructDecl&>(node);
                CollectChildren(decl, children);
                CollectChildren(decl.inheritedTypes, children);
                CollectChildren(decl.body, children);
            })
        .Reg<CollectFunc>(AstKind::ENUM_DECL,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& decl = Cast<const EnumDecl&>(node);
                CollectChildren(decl, children);
                CollectChildren(decl.inheritedTypes, children);
                CollectChildren(decl.constructors, children);
                CollectChildren(decl.members, children);
            })
        .Reg<CollectFunc>(AstKind::EXTEND_DECL,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& decl = Cast<const ExtendDecl&>(node);
                CollectChildren(decl, children);
                CollectChildren(decl.extendedType, children);
                CollectChildren(decl.inheritedTypes, children);
                CollectChildren(decl.members, children);
            })
        .Reg<CollectFunc>(AstKind::INTERFACE_DECL,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& decl = Cast<const InterfaceDecl&>(node);
                CollectChildren(decl, children);
                CollectChildren(decl.inheritedTypes, children);
                CollectChildren(decl.body, children);
            })
        .Reg<CollectFunc>(AstKind::CLASS_DECL,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& decl = Cast<const ClassDecl&>(node);
                CollectChildren(decl, children);
                CollectChildren(decl.inheritedTypes, children);
                CollectChildren(decl.body, children);
            })
        .Reg<CollectFunc>(AstKind::MACRO_DECL,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& decl = Cast<const MacroDecl&>(node);
                CollectChildren(decl, children);
                CollectChildren(decl.funcBody, children);
            })
        .Reg<CollectFunc>(AstKind::FUNC_DECL,
            [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
                auto& decl = Cast<const FuncDecl&>(node);
                CollectChildren(decl, children);
                CollectChildren(decl.funcBody, children);
            })
        .Reg<CollectFunc>(AstKind::MAIN_DECL, [](const AstNode& node, Vec<Ptr<AstNode>>& children) {
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
template <std::derived_from<AstNode> T> inline bool TrySwap(OwnedPtr<T>& dst, OwnedPtr<AstNode>& src)
{
    if (!dst || !src) {
        return false;
    }
    if (T* ptr = dynamic_cast<T*>(src.get().get()); !ptr) {
        return false;
    }
    T* tmp = static_cast<T*>(src.release());
    src.reset(dst.release());
    dst.reset(tmp);
    return true;
}

template <typename T> using OwnedVec = Vec<OwnedPtr<T>>;
using OwnedNodeIter = OwnedVec<AstNode>::iterator;

template <std::derived_from<AstNode> T> inline void ReplaceNode(OwnedPtr<T>& dst, OwnedNodeIter& pos)
{
    if (TrySwap(dst, *pos)) {
        pos++;
    }
}

template <std::derived_from<AstNode> T>
inline void ReplaceRange(OwnedVec<T>& dst, OwnedNodeIter& begin, const OwnedNodeIter& end)
{
    for (auto i = 0; i < dst.size() && begin != end; i++, begin++) {
        TrySwap(dst[i], *begin);
    }
}

inline void ReplaceRange(Decl& decl, OwnedNodeIter& begin)
{
    ReplaceRange(decl.annotations, begin, begin + decl.annotations.size());
    ReplaceNode(decl.annotationsArray, begin);
    ReplaceNode(decl.generic, begin);
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
        .Reg<ReplaceFunc>(AstKind::PACKAGE,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& pkg = Cast<Package&>(node);
                auto it = children.begin();
                ReplaceRange(pkg.files, it, children.end());
            })
        .Reg<ReplaceFunc>(AstKind::PACKAGE_SPEC,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& ps = Cast<PackageSpec&>(node);
                auto it = children.begin();
                ReplaceNode(ps.modifier, it);
            })
        .Reg<ReplaceFunc>(AstKind::IMPORT_SPEC,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& is = Cast<ImportSpec&>(node);
                auto it = children.begin();
                ReplaceNode(is.modifier, it);
            })
        .Reg<ReplaceFunc>(AstKind::INTERFACE_BODY,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& body = Cast<InterfaceBody&>(node);
                auto it = children.begin();
                ReplaceRange(body.decls, it, children.end());
            })
        .Reg<ReplaceFunc>(AstKind::CLASS_BODY,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& body = Cast<ClassBody&>(node);
                auto it = children.begin();
                ReplaceRange(body.decls, it, children.end());
            })
        .Reg<ReplaceFunc>(AstKind::STRUCT_BODY,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& body = Cast<StructBody&>(node);
                auto it = children.begin();
                ReplaceRange(body.decls, it, children.end());
            })
        .Reg<ReplaceFunc>(AstKind::FUNC_BODY,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& body = Cast<FuncBody&>(node);
                auto it = children.begin();
                ReplaceNode(body.paramLists[0], it);
                ReplaceNode(body.generic, it);
                ReplaceNode(body.retType, it);
                ReplaceNode(body.body, it);
            })
        .Reg<ReplaceFunc>(AstKind::FUNC_PARAM_LIST,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& paramList = Cast<FuncParamList&>(node);
                auto it = children.begin();
                ReplaceRange(paramList.params, it, children.end());
            })
        .Reg<ReplaceFunc>(AstKind::FUNC_ARG,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& arg = Cast<FuncArg&>(node);
                auto it = children.begin();
                ReplaceNode(arg.expr, it);
            })
        .Reg<ReplaceFunc>(AstKind::MATCH_CASE_OTHER,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& mco = Cast<MatchCaseOther&>(node);
                OwnedNodeIter it = children.begin();
                ReplaceNode(mco.matchExpr, it);
                ReplaceNode(mco.exprOrDecls, it);
            })
        .Reg<ReplaceFunc>(AstKind::MATCH_CASE,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& mc = Cast<MatchCase&>(node);
                auto it = children.begin();
                ReplaceRange(mc.patterns, it, it + mc.patterns.size());
                ReplaceNode(mc.patternGuard, it);
                ReplaceNode(mc.exprOrDecls, it);
            })
        .Reg<ReplaceFunc>(AstKind::GENERIC_CONSTRAINT,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& gc = Cast<GenericConstraint&>(node);
                auto it = children.begin();
                ReplaceNode(gc.type, it);
                ReplaceRange(gc.upperBounds, it, children.end());
            })
        .Reg<ReplaceFunc>(AstKind::GENERIC,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& generic = Cast<Generic&>(node);
                auto it = children.begin();
                ReplaceRange(generic.typeParameters, it, it + generic.typeParameters.size());
                ReplaceRange(generic.genericConstraints, it, it + generic.genericConstraints.size());
            })
        .Reg<ReplaceFunc>(AstKind::MACRO_EXPAND_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<MacroExpandExpr&>(node);
                auto it = children.begin();
                ReplaceRange(expr.annotations, it, it + expr.annotations.size());
            })
        .Reg<ReplaceFunc>(AstKind::SYNCHRONIZED_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<SynchronizedExpr&>(node);
                auto it = children.begin();
                ReplaceNode(expr.mutex, it);
                ReplaceNode(expr.body, it);
            })
        .Reg<ReplaceFunc>(AstKind::SPAWN_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<SpawnExpr&>(node);
                auto it = children.begin();
                ReplaceNode(expr.futureObj, it);
                ReplaceNode(expr.task, it);
                ReplaceNode(expr.arg, it);
            })
        .Reg<ReplaceFunc>(AstKind::THROW_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<ThrowExpr&>(node);
                auto it = children.begin();
                ReplaceNode(expr.expr, it);
            })
        .Reg<ReplaceFunc>(AstKind::TYPE_CONV_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<TypeConvExpr&>(node);
                auto it = children.begin();
                ReplaceNode(expr.type, it);
                ReplaceNode(expr.expr, it);
            })
        .Reg<ReplaceFunc>(AstKind::DO_WHILE_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<DoWhileExpr&>(node);
                auto it = children.begin();
                ReplaceNode(expr.body, it);
                ReplaceNode(expr.condExpr, it);
            })
        .Reg<ReplaceFunc>(AstKind::FOR_IN_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<ForInExpr&>(node);
                auto it = children.begin();
                ReplaceNode(expr.pattern, it);
                ReplaceNode(expr.patternGuard, it);
                ReplaceNode(expr.inExpression, it);
                ReplaceNode(expr.body, it);
            })
        .Reg<ReplaceFunc>(AstKind::TRAIL_CLOSURE_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<TrailingClosureExpr&>(node);
                auto it = children.begin();
                ReplaceNode(expr.expr, it);
                ReplaceNode(expr.lambda, it);
            })
        .Reg<ReplaceFunc>(AstKind::LAMBDA_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<FuncDecl&>(node);
                auto it = children.begin();
                ReplaceNode(expr.funcBody, it);
            })
        .Reg<ReplaceFunc>(AstKind::WHILE_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<WhileExpr&>(node);
                auto it = children.begin();
                ReplaceNode(expr.condExpr, it);
                ReplaceNode(expr.body, it);
            })
        .Reg<ReplaceFunc>(AstKind::TRY_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<TryExpr&>(node);
                auto it = children.begin();
                ReplaceRange(expr.resourceSpec, it, it + expr.resourceSpec.size());
                ReplaceNode(expr.tryBlock, it);
                ReplaceRange(expr.catchPatterns, it, it + expr.catchPatterns.size());
                ReplaceRange(expr.catchBlocks, it, it + expr.catchBlocks.size());
                ReplaceNode(expr.finallyBlock, it);
            })
        .Reg<ReplaceFunc>(AstKind::QUOTE_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<QuoteExpr&>(node);
                auto it = children.begin();
                ReplaceRange(expr.exprs, it, children.end());
            })
        .Reg<ReplaceFunc>(AstKind::LET_PATTERN_DESTRUCTOR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<LetPatternDestructor&>(node);
                auto it = children.begin();
                ReplaceRange(expr.patterns, it, it + expr.patterns.size());
                ReplaceNode(expr.initializer, it);
            })
        .Reg<ReplaceFunc>(AstKind::IF_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<IfExpr&>(node);
                auto it = children.begin();
                ReplaceNode(expr.condExpr, it);
                ReplaceNode(expr.thenBody, it);
                ReplaceNode(expr.elseBody, it);
            })
        .Reg<ReplaceFunc>(AstKind::BLOCK,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<Block&>(node);
                auto it = children.begin();
                ReplaceRange(expr.body, it, children.end());
            })
        .Reg<ReplaceFunc>(AstKind::MATCH_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<MatchExpr&>(node);
                auto it = children.begin();
                ReplaceNode(expr.selector, it);
                ReplaceRange(expr.matchCases, it, it + expr.matchCases.size());
                ReplaceRange(expr.matchCaseOthers, it, it + expr.matchCaseOthers.size());
            })
        .Reg<ReplaceFunc>(AstKind::TUPLE_LIT,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<TupleLit&>(node);
                auto it = children.begin();
                ReplaceRange(expr.children, it, children.end());
            })
        .Reg<ReplaceFunc>(AstKind::POINTER_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<PointerExpr&>(node);
                auto it = children.begin();
                ReplaceNode(expr.type, it);
                ReplaceNode(expr.arg, it);
            })
        .Reg<ReplaceFunc>(AstKind::ARRAY_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<ArrayExpr&>(node);
                auto it = children.begin();
                ReplaceNode(expr.type, it);
                ReplaceRange(expr.args, it, children.end());
            })
        .Reg<ReplaceFunc>(AstKind::ARRAY_LIT,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<ArrayLit&>(node);
                auto it = children.begin();
                ReplaceRange(expr.children, it, children.end());
            })
        .Reg<ReplaceFunc>(AstKind::RANGE_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<RangeExpr&>(node);
                auto it = children.begin();
                ReplaceNode(expr.startExpr, it);
                ReplaceNode(expr.stopExpr, it);
                ReplaceNode(expr.stepExpr, it);
            })
        .Reg<ReplaceFunc>(AstKind::AS_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<AsExpr&>(node);
                auto it = children.begin();
                ReplaceNode(expr.leftExpr, it);
                ReplaceNode(expr.asType, it);
            })
        .Reg<ReplaceFunc>(AstKind::IS_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<IsExpr&>(node);
                auto it = children.begin();
                ReplaceNode(expr.leftExpr, it);
                ReplaceNode(expr.isType, it);
            })
        .Reg<ReplaceFunc>(AstKind::SUBSCRIPT_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<SubscriptExpr&>(node);
                auto it = children.begin();
                ReplaceNode(expr.baseExpr, it);
                ReplaceRange(expr.indexExprs, it, children.end());
            })
        .Reg<ReplaceFunc>(AstKind::INC_OR_DEC_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<IncOrDecExpr&>(node);
                auto it = children.begin();
                ReplaceNode(expr.expr, it);
            })
        .Reg<ReplaceFunc>(AstKind::BINARY_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<BinaryExpr&>(node);
                auto it = children.begin();
                ReplaceNode(expr.leftExpr, it);
                ReplaceNode(expr.rightExpr, it);
            })
        .Reg<ReplaceFunc>(AstKind::UNARY_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<UnaryExpr&>(node);
                auto it = children.begin();
                ReplaceNode(expr.expr, it);
            })
        .Reg<ReplaceFunc>(AstKind::ASSIGN_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<AssignExpr&>(node);
                auto it = children.begin();
                ReplaceNode(expr.leftValue, it);
                ReplaceNode(expr.rightExpr, it);
            })
        .Reg<ReplaceFunc>(AstKind::STR_INTERPOLATION_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<StrInterpolationExpr&>(node);
                auto it = children.begin();
                ReplaceRange(expr.strPartExprs, it, children.end());
            })
        .Reg<ReplaceFunc>(AstKind::INTERPOLATION_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<InterpolationExpr&>(node);
                auto it = children.begin();
                ReplaceNode(expr.block, it);
            })
        .Reg<ReplaceFunc>(AstKind::LIT_CONST_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<LitConstExpr&>(node);
                auto it = children.begin();
                ReplaceNode(expr.ref, it);
                ReplaceNode(expr.siExpr, it);
            })
        .Reg<ReplaceFunc>(AstKind::RETURN_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<ReturnExpr&>(node);
                auto it = children.begin();
                ReplaceNode(expr.expr, it);
            })
        .Reg<ReplaceFunc>(AstKind::OPTIONAL_CHAIN_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<OptionalChainExpr&>(node);
                auto it = children.begin();
                ReplaceNode(expr.expr, it);
            })
        .Reg<ReplaceFunc>(AstKind::OPTIONAL_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<OptionalExpr&>(node);
                auto it = children.begin();
                ReplaceNode(expr.baseExpr, it);
            })
        .Reg<ReplaceFunc>(AstKind::REF_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<RefExpr&>(node);
                auto it = children.begin();
                ReplaceRange(expr.typeArguments, it, children.end());
            })
        .Reg<ReplaceFunc>(AstKind::MEMBER_ACCESS,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<MemberAccess&>(node);
                auto it = children.begin();
                ReplaceNode(expr.baseExpr, it);
                ReplaceRange(expr.typeArguments, it, children.end());
            })
        .Reg<ReplaceFunc>(AstKind::PAREN_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<ParenExpr&>(node);
                auto it = children.begin();
                ReplaceNode(expr.expr, it);
            })
        .Reg<ReplaceFunc>(AstKind::CALL_EXPR,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& expr = Cast<CallExpr&>(node);
                auto it = children.begin();
                ReplaceNode(expr.baseFunc, it);
                ReplaceRange(expr.args, it, it + expr.args.size());
                ReplaceRange(expr.defaultArgs, it, children.end());
            })
        .Reg<ReplaceFunc>(AstKind::TUPLE_TYPE,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& ty = Cast<TupleType&>(node);
                auto it = children.begin();
                ReplaceRange(ty.fieldTypes, it, children.end());
            })
        .Reg<ReplaceFunc>(AstKind::FUNC_TYPE,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& ty = Cast<FuncType&>(node);
                auto it = children.begin();
                ReplaceRange(ty.paramTypes, it, it + ty.paramTypes.size());
                ReplaceNode(ty.retType, it);
            })
        .Reg<ReplaceFunc>(AstKind::PAREN_TYPE,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& ty = Cast<ParenType&>(node);
                auto it = children.begin();
                ReplaceNode(ty.type, it);
            })
        .Reg<ReplaceFunc>(AstKind::VARRAY_TYPE,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& ty = Cast<VArrayType&>(node);
                auto it = children.begin();
                ReplaceNode(ty.typeArgument, it);
                ReplaceNode(ty.constantType, it);
            })
        .Reg<ReplaceFunc>(AstKind::CONSTANT_TYPE,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& ty = Cast<ConstantType&>(node);
                auto it = children.begin();
                ReplaceNode(ty.constantExpr, it);
            })
        .Reg<ReplaceFunc>(AstKind::OPTION_TYPE,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& ty = Cast<OptionType&>(node);
                auto it = children.begin();
                ReplaceNode(ty.componentType, it);
            })
        .Reg<ReplaceFunc>(AstKind::QUALIFIED_TYPE,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& ty = Cast<QualifiedType&>(node);
                auto it = children.begin();
                ReplaceNode(ty.baseType, it);
                ReplaceRange(ty.typeArguments, it, children.end());
            })
        .Reg<ReplaceFunc>(AstKind::REF_TYPE,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& ty = Cast<RefType&>(node);
                auto it = children.begin();
                ReplaceRange(ty.typeArguments, it, children.end());
            })
        .Reg<ReplaceFunc>(AstKind::EXCEPT_TYPE_PATTERN,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& pat = Cast<ExceptTypePattern&>(node);
                auto it = children.begin();
                ReplaceNode(pat.pattern, it);
                ReplaceRange(pat.types, it, children.end());
            })
        .Reg<ReplaceFunc>(AstKind::TYPE_PATTERN,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& pat = Cast<TypePattern&>(node);
                auto it = children.begin();
                ReplaceNode(pat.pattern, it);
                ReplaceNode(pat.type, it);
            })
        .Reg<ReplaceFunc>(AstKind::VAR_OR_ENUM_PATTERN,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& pat = Cast<VarOrEnumPattern&>(node);
                auto it = children.begin();
                ReplaceNode(pat.pattern, it);
            })
        .Reg<ReplaceFunc>(AstKind::ENUM_PATTERN,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& pat = Cast<EnumPattern&>(node);
                auto it = children.begin();
                ReplaceNode(pat.constructor, it);
                ReplaceRange(pat.patterns, it, children.end());
            })
        .Reg<ReplaceFunc>(AstKind::TUPLE_PATTERN,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& pat = Cast<TuplePattern&>(node);
                auto it = children.begin();
                ReplaceRange(pat.patterns, it, children.end());
            })
        .Reg<ReplaceFunc>(AstKind::CONST_PATTERN,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& pat = Cast<ConstPattern&>(node);
                auto it = children.begin();
                ReplaceNode(pat.literal, it);
                ReplaceNode(pat.operatorCallExpr, it);
            })
        .Reg<ReplaceFunc>(AstKind::VAR_PATTERN,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& pat = Cast<VarPattern&>(node);
                auto it = children.begin();
                ReplaceNode(pat.varDecl, it);
            })
        .Reg<ReplaceFunc>(AstKind::MACRO_EXPAND_DECL,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& decl = Cast<MacroExpandDecl&>(node);
                auto it = children.begin();
                ReplaceRange(decl, it);
                ReplaceNode(decl.invocation.decl, it);
                ReplaceRange(decl.invocation.nodes, it, children.end());
            })
        .Reg<ReplaceFunc>(AstKind::GENERIC_PARAM_DECL,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& decl = Cast<GenericParamDecl&>(node);
                auto it = children.begin();
                ReplaceRange(decl, it);
            })
        .Reg<ReplaceFunc>(AstKind::VAR_WITH_PATTERN_DECL,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& decl = Cast<VarWithPatternDecl&>(node);
                auto it = children.begin();
                ReplaceRange(decl, it);
                ReplaceNode(decl.irrefutablePattern, it);
            })
        .Reg<ReplaceFunc>(AstKind::FUNC_PARAM,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& decl = Cast<FuncParam&>(node);
                auto it = children.begin();
                ReplaceRange(decl, it);
                ReplaceNode(decl.assignment, it);
            })
        .Reg<ReplaceFunc>(AstKind::PROP_DECL,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& decl = Cast<PropDecl&>(node);
                auto it = children.begin();
                ReplaceRange(decl, it);
                ReplaceRange(decl.getters, it, it + decl.getters.size());
                ReplaceRange(decl.setters, it, it + decl.setters.size());
            })
        .Reg<ReplaceFunc>(AstKind::VAR_DECL,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& decl = Cast<VarDecl&>(node);
                auto it = children.begin();
                ReplaceRange(decl, it);
                ReplaceNode(decl.type, it);
                ReplaceNode(decl.initializer, it);
            })
        .Reg<ReplaceFunc>(AstKind::BUILTIN_DECL,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& decl = Cast<BuiltInDecl&>(node);
                auto it = children.begin();
                ReplaceRange(decl, it);
            })
        .Reg<ReplaceFunc>(AstKind::PRIMARY_CTOR_DECL,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& decl = Cast<PrimaryCtorDecl&>(node);
                auto it = children.begin();
                ReplaceRange(decl, it);
                ReplaceNode(decl.funcBody, it);
            })
        .Reg<ReplaceFunc>(AstKind::TYPE_ALIAS_DECL,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& decl = Cast<TypeAliasDecl&>(node);
                auto it = children.begin();
                ReplaceRange(decl, it);
                ReplaceNode(decl.type, it);
            })
        .Reg<ReplaceFunc>(AstKind::STRUCT_DECL,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& decl = Cast<StructDecl&>(node);
                auto it = children.begin();
                ReplaceRange(decl, it);
                ReplaceRange(decl.inheritedTypes, it, it + decl.inheritedTypes.size());
                ReplaceNode(decl.body, it);
            })
        .Reg<ReplaceFunc>(AstKind::ENUM_DECL,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& decl = Cast<EnumDecl&>(node);
                auto it = children.begin();
                ReplaceRange(decl, it);
                ReplaceRange(decl.inheritedTypes, it, it + decl.inheritedTypes.size());
                ReplaceRange(decl.constructors, it, it + decl.constructors.size());
                ReplaceRange(decl.members, it, it + decl.members.size());
            })
        .Reg<ReplaceFunc>(AstKind::EXTEND_DECL,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& decl = Cast<ExtendDecl&>(node);
                auto it = children.begin();
                ReplaceRange(decl, it);
                ReplaceNode(decl.extendedType, it);
                ReplaceRange(decl.inheritedTypes, it, it + decl.inheritedTypes.size());
                ReplaceRange(decl.members, it, it + decl.members.size());
            })
        .Reg<ReplaceFunc>(AstKind::INTERFACE_DECL,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& decl = Cast<InterfaceDecl&>(node);
                auto it = children.begin();
                ReplaceRange(decl, it);
                ReplaceRange(decl.inheritedTypes, it, it + decl.inheritedTypes.size());
                ReplaceNode(decl.body, it);
            })
        .Reg<ReplaceFunc>(AstKind::CLASS_DECL,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& decl = Cast<ClassDecl&>(node);
                auto it = children.begin();
                ReplaceRange(decl, it);
                ReplaceRange(decl.inheritedTypes, it, it + decl.inheritedTypes.size());
                ReplaceNode(decl.body, it);
            })
        .Reg<ReplaceFunc>(AstKind::MACRO_DECL,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& decl = Cast<MacroDecl&>(node);
                auto it = children.begin();
                ReplaceRange(decl, it);
                ReplaceNode(decl.funcBody, it);
            })
        .Reg<ReplaceFunc>(AstKind::FUNC_DECL,
            [](AstNode& node, OwnedVec<AstNode>& children) {
                auto& decl = Cast<FuncDecl&>(node);
                AH_ASSERT(children.size() >= AstNodeHelper::GetChildren(decl).size());
                auto it = children.begin();
                ReplaceRange(decl, it);
                ReplaceNode(decl.funcBody, it);
            })
        .Reg<ReplaceFunc>(AstKind::MAIN_DECL, [](AstNode& node, OwnedVec<AstNode>& children) {
            auto& decl = Cast<MainDecl&>(node);
            auto it = children.begin();
            ReplaceRange(decl, it);
            ReplaceNode(decl.funcBody, it);
        });
}