/**
 * @file
 *
 * This file implements the ToJavaPass.
 */
#include "ToJavaPass.h"

namespace fs = std::filesystem;

REG_PASS("to-java", ([](const PassConfig& config) {
    return UniquePtr<Pass>(new ToJavaPass{Cast<const ToSourcePassConfig&>(config)});
}));

ToJavaPass::ToJavaPass(const ToSourcePassConfig& config) : ToSourcePass(config)
{
    RegisterHandlers();
}

void ToJavaPass::RegisterHandlers()
{
    visitor.RegVisit(
        AstKind::FILE, [this](const AstNode& node, VisitResult& res) { this->Visit(Cast<const File&>(node), res); });
}

void ToJavaPass::Visit(const File& node, VisitResult&)
{
    DEBUG("For File imports: ", node.imports.size());
}
