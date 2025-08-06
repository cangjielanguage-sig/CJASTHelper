/**
 * @file
 *
 * This file declares the Ast2SourceVisitor.
 */
#ifndef AST_2_SOURCE_VISITOR_H
#define AST_2_SOURCE_VISITOR_H
#include "AstVisitor.h"
#include "Macro.h"
#include "Printer.h"
#include <fstream>

// 定义自定义异常类
class Ast2SourceException : public std::exception {
private:
    std::string message;

public:
    explicit Ast2SourceException(const std::string& msg) noexcept;
    const char* what() const noexcept override;
};

class Ast2SourceVisitor : public AstVisitor {
public:
    Ast2SourceVisitor(const std::string& out, int indent = 2);

protected:
    VisitResult Before(const File& node) override;
    void After(const File& node, const VisitResult& res) override;

// 定义重写 Visit 声明的宏
#define GEN_VISIT_OVERRIDE(N) void Visit(const N& node, VisitResult&) override
    // 递归展开需要重写的节点
    EXPAND2(GEN_VISIT_OVERRIDE, Annotation, Modifier);
    EXPAND1(GEN_VISIT_OVERRIDE, File);
    EXPAND4(GEN_VISIT_OVERRIDE, FuncDecl, FuncBody, FuncParamList, FuncParam);
    EXPAND4(GEN_VISIT_OVERRIDE, MainDecl, VarDecl, ClassDecl, ClassBody);
    // Type
    EXPAND2(GEN_VISIT_OVERRIDE, PrimitiveType, RefType);
    // Pattern
    EXPAND4(GEN_VISIT_OVERRIDE, WildcardPattern, ConstPattern, EnumPattern, VarPattern);
    EXPAND3(GEN_VISIT_OVERRIDE, TypePattern, VarOrEnumPattern, TuplePattern);
    // Expr
    EXPAND4(GEN_VISIT_OVERRIDE, Block, FuncArg, MatchCase, MatchCaseOther);
    EXPAND4(GEN_VISIT_OVERRIDE, RefExpr, BinaryExpr, CallExpr, ReturnExpr);
    EXPAND4(GEN_VISIT_OVERRIDE, LitConstExpr, ArrayLit, MemberAccess, LambdaExpr);
    EXPAND4(GEN_VISIT_OVERRIDE, MatchExpr, IsExpr, AsExpr, AssignExpr);

private:
    Printer& PRT();
    using Ty = Cangjie::AST::Ty;
    using Type = Cangjie::AST::Type;
    // 优先使用type, 其次使用ty
    void VisitType(const Ptr<Type> type, const Ptr<Ty> ty = nullptr);
    void VisitTy(const Ty& ty);
    void VisitDecl(const Decl& node);

private:
    std::string out;
    std::fstream ofs;
    Printer prt;
};

#endif // AST_2_SOURCE_VISITOR_H