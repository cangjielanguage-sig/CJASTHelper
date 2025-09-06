#pragma once

#include "PassConfig.h"
#include "wrapper/WrapperAst.h"

class Pass {
public:
    Pass(const PassConfig& config) : config(config)
    {
    }

    virtual ~Pass() = default;
    virtual void Run(AstNode& node) = 0;

protected:
    PassConfig config;
};
