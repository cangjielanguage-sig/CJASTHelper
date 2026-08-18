/**
 * @file
 *
 * This file declares the JSON diagnostic collector.
 *
 * 在 --diagnostic-format=json 且 check-syntax 多包(多目录)检查时, 每个包各一次前端调用,
 * 若用默认 handler, 每包结束都会打印一份 JSON 文档, 多份拼接后不再是合法 JSON。
 * JsonDiagCollector 子类化默认的 CompilerDiagnosticHandler:
 *  - HandleDiagnose 先交给基类收集进集合 —— GetErrorCount() 依赖 handler 集合, 这样
 *    check-syntax 的退出码(是否全部通过)仍然正确;
 *  - 同时把每条诊断格式化为 JSON 字符串聚合到进程级存储(按 文件/行/列 去重);
 *  - 全部包检查结束后由 cjah 统一输出一份合并后的 JSON 文档(按 文件/行/列 排序,
 *    消除默认 handler 内部无序容器迭代导致的输出顺序随机性)。
 * 注意: 通过 RegisterHandler(unique_ptr) 注册的自定义 handler 不会触发默认的逐包打印
 * (探针实测), 因此不会产生重复输出。
 */

#pragma once

#include "cangjie/Basic/DiagnosticEngine.h"
#include <string>
#include <vector>

class JsonDiagCollector : public Cangjie::CompilerDiagnosticHandler {
public:
    explicit JsonDiagCollector(Cangjie::DiagnosticEngine& diag);
    void HandleDiagnose(Cangjie::Diagnostic& d) override;

    /**
     * 若当前会话启用了 JSON 诊断, 输出合并后的 `{"Diags": [...]}` 文档到 stderr,
     * 并按 (文件, 行, 列) 排序后输出; 无诊断时输出空 Diags 数组。非 JSON 模式为 no-op。
     */
    static void FlushIfJsonMode();
};