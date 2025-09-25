#include "utils/TaskExecutor.h"

TaskExecutor::TaskExecutor(size_t concurrency) : concurrency_(concurrency), stop_(false)
{
    if (concurrency == 0) {
        throw std::invalid_argument("Concurrency level must be > 0");
    }

    // 启动 worker 线程
    for (size_t i = 0; i < concurrency_; ++i) {
        workers_.emplace_back([this] { WorkerLoop(); });
    }
}

/**
 * 析构时等待所有任务完成并关闭线程
 */
TaskExecutor::~TaskExecutor()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    for (std::thread& t : workers_) {
        if (t.joinable()) {
            t.join();
        }
    }
}

/**
 * 提交一个无返回值任务（仅执行）
 */
void TaskExecutor::Post(Task task)
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        tasks_.emplace(std::move(task));
    }
    cv_.notify_one();
}

/**
 * 等待所有已提交任务完成
 * 注意：不会阻塞新任务提交
 */
void TaskExecutor::WaitAll()
{
    std::unique_lock<std::mutex> lock(queue_mutex_);
    cv_.wait(lock, [this] { return tasks_.empty(); });
}

/**
 * 获取当前任务队列大小（仅估算）
 */
size_t TaskExecutor::Size() const
{
    std::unique_lock<std::mutex> lock(queue_mutex_);
    return tasks_.size();
}

void TaskExecutor::WorkerLoop()
{
    while (true) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });

            if (stop_ && tasks_.empty()) {
                return;
            }

            if (!tasks_.empty()) {
                task = std::move(tasks_.front());
                tasks_.pop();
            }
        }

        if (task) {
            task();
        }
    }
}
