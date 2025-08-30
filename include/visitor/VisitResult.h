/**
 * @file
 *
 * This file declares the visit result.
 */
#pragma once

// 定义遍历结果结构体
struct VisitResult {
    /**
     * @brief 构造函数，初始化状态。
     *
     * @param status 遍历状态，true 表示继续遍历，false 表示停止遍历。
     */
    VisitResult(bool status);
    /**
     * @brief 获取一个表示继续遍历的结果。
     *
     * @return 表示继续遍历的 VisitResult 对象。
     */
    static VisitResult Cont();
    /**
     * @brief 获取一个表示跳过当前节点的结果。
     *
     * @return 表示跳过当前节点的 VisitResult 对象。
     */
    static VisitResult Skip();
    bool status; // 遍历状态，true 表示继续遍历，false 表示停止遍历
};
