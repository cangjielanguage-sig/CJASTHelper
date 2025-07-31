/**
 * @file
 *
 * This file implementation of AstVisitorBase.
 */

#include "AstVisitorBase.h"

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

VisitResult traverseAst(const AstNode& node, AstVisitorBase& visitor)
{
    auto visitResult = visitor.BeforeVisit(node);
    if (visitResult.status) {
        visitor.VisitChildren(node, visitResult);
        visitor.AfterVisit(node, visitResult);
    }
    return visitResult;
}
