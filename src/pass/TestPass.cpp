#include "pass/TestPass.h"
#include "utils/Cast.h"
#include "utils/Logger.h"

TestPass::TestPass()
{
    RegisterHandler(AstKind::FILE, nullptr, nullptr, nullptr,
        [](AstNode& node, ValuedResult& base, std::vector<ValuedResult>& childrenRes) {
            Logger::Get().Debug("TestPass::MergeResult", "File, childrenRes.size() = ", childrenRes.size());
            std::vector<OwnedPtr<AstNode>> children;
            for (auto& child : childrenRes) {
                if (auto val = child.TryGet<OwnedNodeValue>()) {
                    children.push_back(std::move(*val));
                }
            }
            AstNodeHelper::ReplaceChildren(node, children);
        });
    RegisterHandler(
        AstKind::FUNC_DECL, nullptr,
        [](AstNode& node, ValuedResult& res) {
            Logger::Get().Debug("TestPass::Visit", "FuncDecl");
            FuncDecl& fn = Cast<FuncDecl&>(node);
            OwnedPtr<FuncDecl> magicFn = AstNodeHelper::Clone<FuncDecl>(fn);
            magicFn->identifier = "foo_magic";
            res.Set<OwnedNodeValue>(std::move(magicFn));
        },
        nullptr);
}
