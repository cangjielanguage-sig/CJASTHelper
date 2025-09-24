/**
 * @file
 *
 * This file declares the ToJavaPass.
 */
#pragma once

#include "ToSourcePass.h"

/**
 * @class ToJavaPass
 */
class ToJavaPass : public ToSourcePass {
public:
    /**
     * @brief 构造函数，初始化输出文件、缩进和标志。
     * @param config 配置对象，包含输出文件、缩进和标志信息。
     */
    ToJavaPass(const ToSourcePassConfig& config);
    ~ToJavaPass() override = default;

protected:
#define GEN_BEFORE_OVERRIDE(N) VisitResult Before(const N& node)
#define GEN_VISIT_OVERRIDE(N) void Visit(const N& node, VisitResult&)

    EXPAND1(GEN_VISIT_OVERRIDE, File);

    void RegisterHandlers();
};
