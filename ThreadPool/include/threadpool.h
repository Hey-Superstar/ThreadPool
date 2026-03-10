#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<vector>
#include<queue>
#include<memory>
#include<atomic>
#include<mutex>
#include<condition_variable>
#include<functional>

class Any {

};

class Task {
public:
	//用户自定义任意任务类型，继承Task,实现自定义任务处理
	virtual void run() = 0;

};

enum class PoolMode  //强制要求是用PoolMode::
{
	MODE_FIXED,
	MODE_CACHED,
};

class Thread {	
public:
	using ThreadFunc = std::function<void()>;

	Thread(ThreadFunc func);
	~Thread();
	void start();
private:
	ThreadFunc func_;
};

/*
*
example:
ThreadPool pool;
pool.start(4);
class MyTask : public Task{
public:
	void run(){...}
} 

pool.submit_task(std::make_shared<MyTask>());
*
*/
class ThreadPool {
public:
	ThreadPool();
	~ThreadPool();
	
	void start(size_t init_threadsize = 4);

	//设置任务队列上限阈值
	void set_taskque_max_threshhold(int threshhold);
	
	//给线程池提交任务
	void submit_task(std::shared_ptr<Task> sp);

	void set_mode(PoolMode mode);

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator = (const ThreadPool&) = delete;

private:
	void thread_func();

private:
	std::vector<std::unique_ptr<Thread>> threads_;//线程列表
	size_t init_thread_size;

	std::queue<std::shared_ptr<Task>> taskque_;
	std::atomic<int> task_size;
	int taskque_threshhold; //任务队列数量上限的阈值
	
	std::mutex task_que_mtx;
	//线程通信的条件变量
	std::condition_variable not_full;  //不空
	std::condition_variable not_empty;  //不满

	PoolMode pool_mode; //线程池的工作模式（固定or可增）
};

#endif // !THREADPOOL_H
