#include "visitor/TestPass.h"
#include "utils/Cast.h"
#include "utils/Logger.h"

TestPass::TestPass()
{
    RegisterHandler(AstKind::FILE, nullptr, nullptr, nullptr,
        [](AstNode& node, MutResult& base, std::vector<MutResult>& childrenRes) {
            Logger::Get().Debug("TestPass::MergeResult", "File, childrenRes.size() = ", childrenRes.size());
            std::vector<OwnedPtr<AstNode>> children;
            for (auto& child : childrenRes) {
                if (auto val = child.TryGetNode()) {
                    children.push_back(std::move(*val));
                }
            }
            AstNodeHelper::ReplaceChildren(node, children);
        });
    RegisterHandler(
        AstKind::FUNC_DECL, nullptr,
        [](AstNode& node, MutResult& res) {
            Logger::Get().Debug("TestPass::Visit", "FuncDecl");
            FuncDecl& fn = Cast<FuncDecl&>(node);
            OwnedPtr<FuncDecl> magicFn = AstNodeHelper::Clone<FuncDecl>(fn);
            magicFn->identifier = "foo_magic";
            res.SetNode(std::move(magicFn));
        },
        nullptr);
}
