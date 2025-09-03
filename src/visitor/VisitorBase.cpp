/**
 * @file
 *
 * This file implements the AstVisitorBase.
 */

#include "visitor/VisitorBase.h"
#include "utils/Cast.h"
#include "utils/Logger.h"

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
