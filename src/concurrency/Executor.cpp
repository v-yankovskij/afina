#include <afina/concurrency/Executor.h>
#include <chrono>
#include <algorithm>

namespace Afina {
namespace Concurrency {

Executor::Executor(size_t low_watermark_, size_t high_watermark_, size_t max_queue_size_, int64_t idle_time_): low_watermark(low_watermark_), high_watermark(high_watermark_), max_queue_size(max_queue_size_), idle_time(idle_time_){
    std::unique_lock<std::mutex> lock(mutex);
    state = State::kRun;
    running_threads = 0;
    for(size_t i; i < low_watermark; i++){
        threads.emplace_back(std::thread([this] { return perform();}));
    }
}

Executor::~Executor(){
    Stop(true);
}

void Executor::Stop(bool await){
    std::unique_lock<std::mutex> lock(mutex);
    if (state == State::kStopped){
        return;
    }
    state = State::kStopping;
    empty_condition.notify_all();
    while(!threads.empty() && await){
        stopping_condition.wait(lock);
    }
    if (threads.empty()){
        state = State::kStopped;
        return;
    }
}

void Executor::perform(){
    std::unique_lock<std::mutex> lock(mutex);
    while (state == State::kRun){
        if (tasks.empty()){
            auto start = std::chrono::steady_clock::now();
            auto result = empty_condition.wait_until(lock, start + std::chrono::milliseconds(idle_time));
            if (result == std::cv_status::timeout && threads.size() > low_watermark){
                break;
            } else {
                continue;
            }
        }
        auto task = std::move(tasks.front());
        tasks.pop_front();
        running_threads++;
        lock.unlock();
        try {
            task();
        } catch (std::exception &ex) {
            _logger->error(ex.what());    
        }
        lock.lock();
        running_threads--;
    }
    auto this_thread = std::this_thread::get_id();
    auto it = std::find_if(threads.begin(), threads.end(), [this_thread](std::thread &x){return x.get_id() == this_thread; });
    if (threads.empty()){
        stopping_condition.notify_all();
    }
    threads.erase(it);
}

} // namespace Concurrency
} // namespace Afina
