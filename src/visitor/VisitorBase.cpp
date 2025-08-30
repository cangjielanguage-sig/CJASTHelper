/**
 * @file
 *
 * This file implementation of AstVisitorBase.
 */

#include "visitor/AstVisitorBase.h"
#include "visitor/MutAstVisitorBase.h"

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

VisitResult MutTraverse(AstNode& node, MutAstVisitorBase& visitor)
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
