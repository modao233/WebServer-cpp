#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <assert.h>
#include <functional>

typedef std::function<void()> func;

typedef struct Pool {
    std::mutex mtx;
    std::condition_variable cond;
    bool is_closed;
    std::queue<func> tasks;
}Pool;

class ThreadPool {
public:
    explicit ThreadPool(size_t thread_count = 8):pool_ptr_(std::make_shared<Pool>()){
        assert(thread_count > 0);
        for(size_t i = 0; i < thread_count; ++i){
            std::thread([pool=pool_ptr_]{
                std::unique_lock<std::mutex> locker(pool->mtx);
                while(true){
                    if(!pool->tasks.empty()){
                        auto task = std::move(pool->tasks.front());
                        pool->tasks.pop();
                        locker.unlock();
                        task();
                        locker.lock();
                    }
                    else if(pool->is_closed)break;
                    else pool->cond.wait(locker);
                }
            }).detach();
        }
    }

    ThreadPool() = default;
    ThreadPool(ThreadPool&&) = default;
    ~ThreadPool(){
        if(static_cast<bool>(pool_ptr_)){
            std::lock_guard<std::mutex> locker(pool_ptr_->mtx);
            pool_ptr_->is_closed = true;
        }
        pool_ptr_->cond.notify_all();
    }

    template<class F>
    void AddTask(F&& task){
        {
            std::lock_guard<std::mutex> locker(pool_ptr_->mtx);
            pool_ptr_->tasks.emplace(std::forward<F>(task));
        }
        pool_ptr_->cond.notify_one();
    }
private:
    std::shared_ptr<Pool> pool_ptr_;
};
#endif