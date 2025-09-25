/**
 * @file
 *
 * This file declares the ToSourcePass.
 */
#pragma once

#include "core/pass/Pass.h"
#include "core/visitor/ConstAstVisitor.h"
#include "utils/Cast.h"
#include "utils/FileHelper.h"
#include "utils/Macro.h"
#include "utils/Printer.h"
#include <fstream>

/**
 * @class ToSourcePass
 * @brief 依赖 `ConstAstVisitor`，用于将AST转换为源代码。
 */
class ToSourcePass : public Pass {
public:
    /**
     * @brief 构造函数，初始化输出文件、缩进和标志。
     * @param config 配置对象，包含输出文件、缩进和标志信息。
     */
    ToSourcePass(const ToSourcePassConfig& config) : Pass(config), prt(ofs, Config().indent)
    {
    }
    ~ToSourcePass() override = default;

    void Run(AstNode& node) override
    {
        CreateDirIfNotExists(Config().out);
        (void)Traverse(node, visitor);
    }

protected:
    /**
     * @brief 遍历单个节点。
     *
     * @tparam Ptr 指针类型模板。
     * @tparam T 节点的具体类型。
     * @param pnode 要遍历的节点指针。
     */
    template <template <typename> class Ptr, typename T> inline void VisitNode(const Ptr<T>& pnode)
    {
        if (pnode) {
            Traverse(*pnode, visitor);
        }
    }

    /**
     * @brief 遍历一组节点。
     *
     * @tparam Ptr 指针类型模板。
     * @tparam T 节点的具体类型。
     * @param nodes 要遍历的节点指针数组。
     */
    template <template <typename> class Ptr, typename T> inline void VisitNodes(ConVec<Ptr<T>>& nodes)
    {
        for (auto& node : nodes) {
            VisitNode(node, visitor);
        }
    }

protected:
    /**
     * @brief 获取 `Printer` 实例。
     * @return `Printer` 的引用。
     */
    Printer& PRT()
    {
        return prt;
    }

    const ToSourcePassConfig& Config() const
    {
        return Cast<const ToSourcePassConfig&>(config);
    }

protected:
    std::fstream ofs;                                          /**< 输出文件流 */
    Printer prt;                                               /**< 打印器实例 */
    UnorderedMap<Ptr<const Decl>, std::string> desugaredVarId; /**< 解糖变量名字表 */
    ConstAstVisitor visitor;                                   /**< 抽象语法树遍历器 */
};
