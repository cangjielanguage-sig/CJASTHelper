/**
 * @file
 *
 * This file declares the wrapper ast nodes.
 */
#pragma once

#include "cangjie/AST/Node.h"
#include "utils/Macro.h"

using AstNode = Cangjie::AST::Node;

using AstKind = Cangjie::AST::ASTKind;
using Cangjie::Identifier;
using Cangjie::TokenKind;
using Cangjie::AST::Attribute;
using Cangjie::AST::CallKind;
using Cangjie::AST::Decl;
using Cangjie::AST::Expr;
using Cangjie::AST::ImportKind;
using Cangjie::AST::InheritableDecl;
using Cangjie::AST::NameReferenceExpr;
using Cangjie::AST::Pattern;
using Cangjie::AST::Ty;
using Cangjie::AST::Type;
using Cangjie::AST::VarDeclAbstract;

// 宏自动生成 using Cangjie::AST::Package
#define AST_INFO(KIND, STR, DEF) using Cangjie::AST::DEF;
#include "AstInfo.inc"
#undef AST_INFO

std::string AstKind2Str(AstKind kind);