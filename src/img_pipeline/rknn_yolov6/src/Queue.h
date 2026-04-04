#ifndef Queue_H
#define Queue_H

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

template<typename T>
class Queue
{
public:
    void push(const T& value)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if(queue_.size()>=2)
            queue_.pop();
        
        queue_.push(value);
        cond_var_.notify_one();
    }

    T wait_and_pop()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while(queue_.empty())
        {
            cond_var_.wait(lock);
        }
        auto value = queue_.front();
        queue_.pop();
        return value;
    }

    void wait_and_pop(T& value)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while(queue_.empty())
        {
            cond_var_.wait(lock);
        }
        value = queue_.front();
        queue_.pop();
    }

private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cond_var_;
};

#endif
