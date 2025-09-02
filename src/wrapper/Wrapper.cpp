/**
 * @file
 *
 * This file implements the wrapper ast nodes.
 */

#include "wrapper/WrapperAst.h"

std::string AstKind2Str(AstKind kind)
{
    static std::unordered_map<AstKind, std::string> kindsInfo{
#define AST_INFO(KIND, STR, DEF) {AstKind::KIND, STR},
#include "wrapper/AstInfo.inc"
#undef AST_INFO
    };
    return kindsInfo.at(kind);
}
