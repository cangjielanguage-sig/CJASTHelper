/**
 * @file
 *
 * This file declares the wrapper ast nodes.
 */
#pragma once
#include "TypeAlias.h"
#include "cangjie/AST/Clone.h"
#include "utils/CallbackManger.h"
#include "utils/types/TypeAlias.h"

using AstCloner = Cangjie::AST::ASTCloner;

Str AstKind2Str(AstKind kind);

/**
 * TokenKind 映射字符串辅助函数
 */
inline Str Tk2Str(TokenKind tk)
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
    static Vec<Ptr<AstNode>> GetChildren(const AstNode& node);

    static void ReplaceChildren(AstNode& node, Vec<OwnedPtr<AstNode>>& children);

    template <typename T> static inline OwnedPtr<T> Clone(T& node)
    {
        return AstCloner::Clone<T>(&node);
    }

    static void DumpAst(const AstNode& node, ConStr&);

    using CollectFunc = Function<void(const AstNode&, Vec<Ptr<AstNode>>&)>;
    using ReplaceFunc = Function<void(AstNode&, Vec<OwnedPtr<AstNode>>&)>;

private:
    AstNodeHelper();

    void RegCollectHandlers();
    void RegReplaceHandlers();

private:
    static AstNodeHelper& GetInstance();
    static UniquePtr<AstNodeHelper> helper;

    CallbackManager<AstKind, Tuple<CollectFunc, ReplaceFunc>> handlers;
};
