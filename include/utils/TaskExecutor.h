// TaskExecutor.hpp
#pragma once
#include "utils/types/TypeAlias.h"

#include <condition_variable>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>

class TaskExecutor {
public:
    using Task = Function<void()>;

    /**
     * 构造任务执行器
     * @param concurrency 并发线程数
     */
    explicit TaskExecutor(size_t concurrency);

    /**
     * 析构时等待所有任务完成并关闭线程
     */
    virtual ~TaskExecutor();

    // 禁止拷贝
    TaskExecutor(const TaskExecutor&) = delete;
    TaskExecutor& operator=(const TaskExecutor&) = delete;

    /**
     * 提交一个任务
     * @param func 可调用对象
     * @param args 参数
     * @return std::future 获取返回值
     */
    template <typename Func, typename... Args>
    auto Submit(Func&& func, Args&&... args) -> std::future<std::invoke_result_t<Func, Args...>>
    {
        using ReturnType = std::invoke_result_t<Func, Args...>;

        // 包装任务，带返回值
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...));

        std::future<ReturnType> result = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            tasks_.emplace([task]() { (*task)(); });
        }
        cv_.notify_one(); // 唤醒一个 worker
        return result;
    }

    /**
     * 提交一个无返回值任务（仅执行）
     */
    void Post(Task task);

    /**
     * 等待所有已提交任务完成
     * 注意：不会阻塞新任务提交
     */
    void WaitAll();
    /**
     * 获取当前任务队列大小（仅估算）
     */
    size_t Size() const;

private:
    void WorkerLoop();

private:
    size_t concurrency_;
    Vec<std::thread> workers_;
    Queue<Task> tasks_;

    mutable std::mutex queue_mutex_;
    std::condition_variable cv_;
    bool stop_;
};
