#ifndef THREAD_POOL_H
#define THREAD_POOL_H
 
#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
 
class ThreadPool {
public:
    ThreadPool(size_t);
    template<class F, class... Args>////模板不支持分离式编译;只好将类和实现写在一起
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;
    ~ThreadPool();
private:
    // need to keep track of threads so we can join them
    std::vector< std::thread > workers;
    // the task queue
    std::queue< std::function<void()> > tasks;
    
    // synchronization
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};
 
// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(size_t threads)
    :   stop(false)
{
    for(size_t i = 0;i<threads;++i)
        //捕获this表示函数体内可以使用Lambda所在类中的成员变量
        workers.emplace_back(
            [this]
            {
                for(;;)
                {
                    std::function<void()> task;
                    //C++11以后在标准库里引入了std::function模板类，这个模板概括了函数指针的概念,函数指针只能指向一个函数，而std::function对象可以代表任何可以调用的对象，比如说任何可以被当作函数一样调用的对象
                    {//这个是为了说明锁的作用域
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock,
                            [this]{ return this->stop || !this->tasks.empty(); });//在线程被阻塞时,该函数会自动调用queue_mutex.unlock() 释放锁，使得其他被阻塞在锁竞争上的线程得以继续执行;一旦当前线程获得通知(notified，通常是另外某个线程调用 notify_(one/all)唤醒了当前线程),wait()函数也是自动调用queue_mutex.lock(),使得queue_mutex的状态和wait函数被调用时相同
                        if(this->stop && this->tasks.empty())
                            return;
                        task = std::move(this->tasks.front());//移动语义,将左值引用转换为右值引用
                        this->tasks.pop();
                    }
 
                    task();
                }
            }
        );
}
 
// add new work item to the pool
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;//这是返回结果的类型
 
    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );//make_shared创建一个智能指针;将函数handle与参数绑定;std::packaged_task它允许传入一个函数，并将函数计算的结果传递给std::future，包括函数运行时产生的异常
        
    std::future<return_type> res = task->get_future();
    {//get_future函数仅能调用一次，多次调用会触发std::future_error异常。
        std::unique_lock<std::mutex> lock(queue_mutex);
 
        // don't allow enqueueing after stopping the pool
        if(stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");
 
        tasks.emplace([task](){ (*task)(); });//把任务压入队列
    }
    condition.notify_one();//notify是通知一个线程获取锁，notifyAll是通知所有相关的线程去竞争锁;notify_one()(随机唤醒一个等待的线程)和notify_all()(唤醒所有等待的线程)//因为任务队列里有了任务,所以
    return res;
}
 
// the destructor joins all threads
inline ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;//stop变成了true 此时通知所有线程
    }
    condition.notify_all();//等待所有的线程任务执行完成退出
    for(std::thread &worker: workers)
        worker.join();
}
 
#endif
