#ifndef AFINA_CONCURRENCY_EXECUTOR_H
#define AFINA_CONCURRENCY_EXECUTOR_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include <spdlog/logger.h>

namespace Afina {
namespace Concurrency {

/**
 * # Thread pool
 */
class Executor {
public:

    enum class State {
        // Threadpool is fully operational, tasks could be added and get executed
        kRun,

        // Threadpool is on the way to be shutdown, no ned task could be added, but existing will be
        // completed as requested
        kStopping,

        // Threadpool is stopped
        kStopped
    };

    Executor(size_t low_watermark_ = 4, size_t high_watermark_ = 16, size_t max_queue_size_ = 100, int64_t idle_time_ = 10000);
    ~Executor();

    /**
     * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
     * free. All enqueued jobs will be complete.
     *
     * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
     */
    void Stop(bool await = false);


    /**
     * Main function that all pool threads are running. It polls internal task queue and execute tasks
     */
    void perform();

    /**
     * Add function to be executed on the threadpool. Method returns true in case if task has been placed
     * onto execution queue, i.e scheduled for execution and false otherwise.
     *
     * That function doesn't wait for function result. Function could always be written in a way to notify caller about
     * execution finished by itself
     */
    template <typename F, typename... Types> bool Execute(F &&func, Types... args) {
        // Prepare "task"
        auto exec = std::bind(std::forward<F>(func), std::forward<Types>(args)...);

        std::unique_lock<std::mutex> lock(this->mutex);
        if (state != State::kRun || tasks.size() >= max_queue_size) {
            return false;
        }

        // Enqueue new task
        if (threads.size() == running_threads && running_threads < high_watermark) {
            threads.emplace_back(std::thread([this] { return perform();}));
        }
        tasks.push_back(exec);
        empty_condition.notify_one();
        return true;
    }

private:
    // No copy/move/assign allowed
    Executor(const Executor &);            // = delete;
    Executor(Executor &&);                 // = delete;
    Executor &operator=(const Executor &); // = delete;
    Executor &operator=(Executor &&);      // = delete;


    /**
     * Mutex to protect state below from concurrent modification
     */
    std::mutex mutex;

    /**
     * Conditional variable to await new data in case of empty queue
     */
    std::condition_variable empty_condition;

    /**
     * Conditional variable to await the stop of executor
     */
    std::condition_variable stopping_condition;

    /**
     * Vector of actual threads that perform execution
     */
    std::vector<std::thread> threads;

    /**
     * Task queue
     */
    std::deque<std::function<void()>> tasks;

    /**
     * Flag to stop bg threads
     */
    State state;

    /**
     * Minimal number of threads available
     */
    size_t low_watermark;

    /**
     * Maximal number of threads available
     */
    size_t high_watermark;

    /**
     * Number of threads currently running
     */
    size_t running_threads;

    /**
     * Maximal number of tasks in queue
     */
    size_t max_queue_size;

    /**
     * Time to wait until a thread is stopped if the number of threads is above low watermark
     */
    int64_t idle_time;
    
    /**
     * Error log
     */
    std::shared_ptr<spdlog::logger> _logger;
};

} // namespace Concurrency
} // namespace Afina

#endif // AFINA_CONCURRENCY_EXECUTOR_H
