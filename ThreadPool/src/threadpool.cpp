#include "../include/threadpool.h"

#include<thread>
#include<iostream>

const int TASK_MAX_THRESHHOLD = 1024;

ThreadPool::ThreadPool()
	: init_thread_size(0)
	, task_size(0)
	, taskque_threshhold(TASK_MAX_THRESHHOLD)
	, pool_mode(PoolMode::MODE_FIXED)
	, is_started(false)
	,idle_thread_size(0)
{}

ThreadPool::~ThreadPool() 
{}

//设置任务队列上限阈值
void ThreadPool::set_taskque_max_threshhold(int threshhold) {
	if (check_pool_running_state()) {
		return;
	}
	taskque_threshhold = threshhold;
}

bool ThreadPool::check_pool_running_state() const{
	return is_started;
}

void ThreadPool::set_mode(PoolMode mode) {
	if(check_pool_running_state()){
		return;
	}
	pool_mode = mode;
}

//给线程池提交任务（生产任务）
Result ThreadPool::submit_task(std::shared_ptr<Task> sp) {

	//获取锁
	std::unique_lock<std::mutex> lock(task_que_mtx);

	//线程的通信 等待任务队列有空余
	//用户题交任务，最长不能超过一秒，否则判断任务题交失败，返回
	if (!not_full.wait_for(lock, std::chrono::seconds(1),
		[&]()->bool {return taskque_.size() < (size_t)taskque_threshhold; })) {
		std::cerr << "taskque is full,submit task fail." << std::endl;
		return Result(sp,false);
	}

	//有空余放入
	taskque_.emplace(sp);
	task_size++;

	//放了任务后 任务队列不空，not_empty通知消费者thread_func
	not_empty.notify_all();

	//cached模式 任务处理速度快，任务积压多了，线程池可以增加线程数量
	if (pool_mode == PoolMode::MODE_CACHED
		&& task_size > idle_thread_size
		&& threads_.size() < 2 * init_thread_size
		) {
		//创建新线程
		std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::thread_func, this));
		//线程对象需要线程函数,要把非静态成员线程函数(thread_func)给他，要bind（非静态成员函数：必须依附于一个具体的对象实例才能调用）
		threads_.emplace_back(std::move(ptr));
	}

	return Result(sp);
}

void ThreadPool::start(size_t init_threadsize) {
	
	is_started = true;

	//初始线程个数
	init_thread_size = init_threadsize;

	//创建线程对象
	for (int i = 0; i < init_thread_size; i++) {
		std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::thread_func, this));
		//线程对象需要线程函数,要把非静态成员线程函数(thread_func)给他，要bind（非静态成员函数：必须依附于一个具体的对象实例才能调用）
		threads_.emplace_back(std::move(ptr));
	}

	//启动所有线程
	for (int i = 0; i < init_thread_size; i++) {
		threads_[i]->start();
		idle_thread_size++;
	}
}

Task::Task() : result_(nullptr){}

void Task::exec() {
	if(result_!=nullptr)
		result_->set_val(run());
}

void Task::set_result(Result* res) {
	result_ = res;
}

//线程函数：线程池的所有线程从任务队列里面（消费任务）
void ThreadPool::thread_func() {
	/*
	std::cout << "begin thread_func tid is" << std::this_thread::get_id() << std::endl;
	std::cout << "end thread_func" << std::endl;
	*/
	
	for (;;) {
		std::shared_ptr<Task> task;
		{	
			std::unique_lock<std::mutex> lock(task_que_mtx);
			std::cout << "尝试获取任务" << std::endl;
			not_empty.wait(lock, [&]()->bool {return taskque_.size() > 0; });
			idle_thread_size--;
			std::cout << "获取任务成功" << std::endl;
			task = taskque_.front();
			taskque_.pop();
			task_size--;

			if (taskque_.size() > 0) {
				not_empty.notify_all();
			}

			//可以继续生产任务了
			not_full.notify_all();
		}
		if (task != nullptr) {
			//task->run();
			task->exec();
		}
		idle_thread_size++;
	}
}


Thread::Thread(ThreadFunc func) 
	:func_(func)
{}

Thread::~Thread() {

}

void Thread::start() {
	std::thread t(func_); //线程对象t,线程函数func
	t.detach();
}

Result::Result(std::shared_ptr<Task> task, bool isvalid)
	:is_valid(isvalid)
	, task_(task)
{
	task_->set_result(this);
}

Any Result::get() {
	if (!is_valid) {
		return "";
	}

	sem_.wait();
	return std::move(any_);
}

void Result::set_val(Any any) {
	this->any_ = std::move(any);
	sem_.post();
}