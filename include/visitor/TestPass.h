/**
 * @file
 *
 * This file declares the TestPass of MutAstVisitor.
 */

#pragma once

#include "MutAstVisitor.h"

class TestPass : public MutAstVisitor {
public:
    TestPass();
    ~TestPass() override = default;
};
