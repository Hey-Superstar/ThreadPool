#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<vector>
#include<queue>
#include<memory>
#include<atomic>
#include<mutex>
#include<condition_variable>
#include<functional>

//线程通信的信号量类
class Semaphore {
public:
	Semaphore(int limit = 0) :res_limit(0) {}
	~Semaphore() = default;

	void wait() {
		std::unique_lock<std::mutex> lock(mtx_);
		//等待信号量有资源，没有资源的话，阻塞当前线程
		cond_.wait(lock, [&]()->bool {return res_limit > 0; });
		res_limit--;
	}

	void post() {
		std::unique_lock<std::mutex> lock(mtx_);
		res_limit++;
		cond_.notify_all();
	}
private:
	int res_limit;
	std::mutex mtx_;;
	std::condition_variable	cond_;
};

//接收任意数据的类型
class Any {
public:
	Any() = default;
	~Any() = default;
	Any(const Any& other) = delete;
	Any& operator=(const Any& other) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;

	template<typename T>
	Any(T data) :base_(std::make_unique<Derive<T>>(data)) {}   //构造函数模板，接收任意类型数据，创建一个Derive对象，存储在base_中

	template<typename T>
	T cast_() {
		Derive<T>* ptr = dynamic_cast<Derive<T>*>(base_.get());
		if (ptr) {
			return ptr->data_;
		}
		throw std::bad_cast();
	}

private:
	class Base {
	public:
		virtual ~Base() = default;
	};
	template<typename T>
	class Derive : public Base {
	public:
		Derive(T data) :data_(data){}
		T data_;
	};
private:
	std::unique_ptr<Base> base_;
};

//提交到线程池的task任务执行完成后的返回值类型Result
class Task;
class Result {
public:
	Result(std::shared_ptr<Task> task,bool isvalid=true);
	~Result() = default;

	void set_val(Any any);
	Any get();
private:
	Any any_;
	Semaphore sem_;
	std::shared_ptr<Task> task_;
	std::atomic<bool> is_valid;
};

class Task {
public:
	//用户自定义任意任务类型，继承Task,实现自定义任务处理
	//如何设计run函数的返回值，可以表示任意的类型
	//C++17 Any类型
	Task();
	~Task() = default;
	void exec();
	void set_result(Result* res);
	virtual Any run() = 0;
private:
	Result* result_;
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
	Result submit_task(std::shared_ptr<Task> sp);

	void set_mode(PoolMode mode);

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator = (const ThreadPool&) = delete;

private:
	void thread_func();

	bool check_pool_running_state() const;
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

	//表示线程池是否已经启动了
	std::atomic<bool> is_started;

	//空闲线程的数量
	std::atomic<int> idle_thread_size;

};

#endif // !THREADPOOL_H
