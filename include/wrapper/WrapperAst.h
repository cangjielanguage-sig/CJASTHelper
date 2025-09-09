/**
 * @file
 *
 * This file declares the wrapper ast nodes.
 */
#pragma once

#include "cangjie/AST/Clone.h"
#include "cangjie/AST/Node.h"
#include "utils/CallbackManger.h"
#include <memory>

using AstNode = Cangjie::AST::Node;
using AstKind = Cangjie::AST::ASTKind;
using Cangjie::Identifier;
using Cangjie::TokenKind;
using Cangjie::AST::Attribute;
using Cangjie::AST::CallKind;
using Cangjie::AST::Decl;
using Cangjie::AST::Expr;
using Cangjie::AST::ForInKind;
using Cangjie::AST::FuncTy;
using Cangjie::AST::ImportKind;
using Cangjie::AST::InheritableDecl;
using Cangjie::AST::NameReferenceExpr;
using Cangjie::AST::Pattern;
using Cangjie::AST::Ty;
using Cangjie::AST::Type;
using Cangjie::AST::TypeKind;
using Cangjie::AST::VarDeclAbstract;

using AstCloner = Cangjie::AST::ASTCloner;

// 宏自动生成 using Cangjie::AST::Package
#define AST_INFO(KIND, STR, DEF) using Cangjie::AST::DEF;
#include "AstInfo.inc"
#undef AST_INFO

std::string AstKind2Str(AstKind kind);

/**
 * TokenKind 映射字符串辅助函数
 */
inline std::string Tk2Str(TokenKind tk)
{
    return Cangjie::TOKENS[static_cast<int>(tk)];
}

class AstNodeHelper {
public:
    /**
     * @brief 获取节点的子节点列表。
     *
     * @param node 要获取子节点的节点。
     * @return 子节点列表。
     */
    static std::vector<Ptr<AstNode>> GetChildren(const AstNode& node);

    static void ReplaceChildren(AstNode& node, std::vector<OwnedPtr<AstNode>>& children);

    template <typename T> static inline OwnedPtr<T> Clone(T& node)
    {
        return AstCloner::Clone<T>(&node);
    }

    static void DumpAst(const AstNode& node, const std::string& out);

    using CollectFunc = std::function<void(const AstNode&, std::vector<Ptr<AstNode>>&)>;
    using ReplaceFunc = std::function<void(AstNode&, std::vector<OwnedPtr<AstNode>>&)>;

private:
    AstNodeHelper();

    void RegCollectHandlers();
    void RegReplaceHandlers();

private:
    static AstNodeHelper& GetInstance();
    static std::unique_ptr<AstNodeHelper> helper;

    CallbackManager<AstKind, std::tuple<CollectFunc, ReplaceFunc>> handlers;
};
