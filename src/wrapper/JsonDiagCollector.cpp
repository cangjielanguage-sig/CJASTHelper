/**
 * @file
 *
 * This file implements the JSON diagnostic collector.
 */

#include "wrapper/JsonDiagCollector.h"
#include "cangjie/Basic/DiagnosticJsonFormatter.h"
#include <algorithm>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>

namespace {
struct DiagItem {
    std::string file;
    long line;
    long col;
    std::string json; // FormatDiagnosticToJson 产出的单条诊断 JSON(含缩进)
};

std::mutex& gMutex()
{
    static std::mutex mtx;
    return mtx;
}

bool& gJsonMode()
{
    static bool jsonMode = false;
    return jsonMode;
}

bool& gSessionStarted()
{
    static bool started = false;
    return started;
}

std::vector<DiagItem>& gStore()
{
    static std::vector<DiagItem> store;
    return store;
}

void ResetSession()
{
    std::lock_guard<std::mutex> lock(gMutex());
    gStore().clear();
    gJsonMode() = false;
    gSessionStarted() = false;
}
} // namespace

JsonDiagCollector::JsonDiagCollector(Cangjie::DiagnosticEngine& diag)
    : Cangjie::CompilerDiagnosticHandler(diag, false, false)
{
    // 多包检查: 每个包构造一次收集器, 仅首个收集器重置进程级存储, 后续收集器追加
    std::lock_guard<std::mutex> lock(gMutex());
    if (!gSessionStarted()) {
        gStore().clear();
        gJsonMode() = true;
        gSessionStarted() = true;
    }
}

void JsonDiagCollector::HandleDiagnose(Cangjie::Diagnostic& myDiag)
{
    // 先交给基类: GetErrorCount() 依赖 handler 收集的集合, 保证 check-syntax 退出码正确
    Cangjie::CompilerDiagnosticHandler::HandleDiagnose(myDiag);
    try {
        Cangjie::DiagnosticJsonFormatter formatter(diag);
        auto entry = formatter.FormatDiagnosticToJson(myDiag, 3);
        if (entry.empty()) {
            return;
        }
        auto meta = nlohmann::json::parse(entry);
        const auto& loc = meta.at("Location");
        std::lock_guard<std::mutex> lock(gMutex());
        DiagItem item{loc.at("File").get<std::string>(), loc.at("Line").get<long>(), loc.at("Column").get<long>(),
            std::move(entry)};
        // 按 文件/行/列 去重(基类亦按位置去重)
        auto& store = gStore();
        auto dup = std::any_of(store.begin(), store.end(), [&item](const DiagItem& x) {
            return x.file == item.file && x.line == item.line && x.col == item.col;
        });
        if (!dup) {
            store.push_back(std::move(item));
        }
    } catch (...) {
        // 单条格式化失败不影响其它诊断
    }
}

void JsonDiagCollector::FlushIfJsonMode()
{
    std::vector<DiagItem> items;
    {
        std::lock_guard<std::mutex> lock(gMutex());
        if (!gJsonMode()) {
            return;
        }
        items = gStore();
    }
    std::sort(items.begin(), items.end(), [](const DiagItem& a, const DiagItem& b) {
        if (a.file != b.file)
            return a.file < b.file;
        if (a.line != b.line)
            return a.line < b.line;
        return a.col < b.col;
    });
    // 与默认 handler 一致, 诊断统一输出到 stderr
    std::cerr << "{\r\n    \"Diags\":[\r\n";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) {
            std::cerr << ",\r\n";
        }
        std::cerr << items[i].json;
    }
    std::cerr << "\r\n    ]\r\n}\r\n";
    ResetSession();
}