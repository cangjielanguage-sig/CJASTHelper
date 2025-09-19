/**
 * @file
 *
 * This file include the all implementations of passes.
 */

#pragma once

#include "core/pass/DesugarPass.h"
#include "core/pass/ToSourcePass.h"
#include "utils/Cast.h"

// 注意：这里使用 c++20 inline static 避免在cpp文件中全局变量初始化不被执行问题
REG_PASS("replace-desugar",
    ([](const PassConfig& config) { return std::unique_ptr<Pass>(new ReplaceDesugarPass{config}); }));
REG_PASS(
    "check-desugar", ([](const PassConfig& config) { return std::unique_ptr<Pass>(new CheckDesugarPass{config}); }));
REG_PASS("to-source", ([](const PassConfig& config) {
    return std::unique_ptr<Pass>(new ToSourcePass{Cast<const ToSourcePassConfig&>(config)});
}));
