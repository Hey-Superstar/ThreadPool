#include "../include/threadpool.h"

#include<thread>
#include<iostream>

const int TASK_MAX_THRESHHOLD = 4;
const int THREAD_MAX_THRESHHOLD = 10;
const int THREAD_IDLE_TIME = 60;

ThreadPool::ThreadPool()
	: init_thread_size(0)
	, task_size(0)
	, idle_thread_size(0)
	, cur_thread_size(0)
	, taskque_threshhold(TASK_MAX_THRESHHOLD)
	, max_thread_size(THREAD_MAX_THRESHHOLD)
	, pool_mode(PoolMode::MODE_FIXED)
	, is_started(false)
{}

void ThreadPool::set_thread_max_threshhold(int threshhold) {
	if (check_pool_running_state()) {
		return;
	}
	if (pool_mode == PoolMode::MODE_CACHED) {
		max_thread_size = threshhold;
	}
}

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

	//根据任务数量和线程数量的关系，决定是否要增加线程数量
	//cached模式 任务场景：任务处理速度快，任务积压多了，线程池可以增加线程数量
	if (pool_mode == PoolMode::MODE_CACHED
		&& task_size > idle_thread_size
		&& cur_thread_size < max_thread_size
		) {
		//创建新线程
		std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::thread_func, this, std::placeholders::_1));
		//线程对象需要线程函数,要把非静态成员线程函数(thread_func)给他，要bind（非静态成员函数：必须依附于一个具体的对象实例才能调用）
		int thread_id = ptr->get_id();
		threads_.emplace(thread_id, std::move(ptr));
		//启动线程
		threads_[thread_id]->start();
		cur_thread_size++;
		idle_thread_size++;
	}

	return Result(sp);
}

void ThreadPool::start(int init_threadsize) {
	
	is_started = true;

	//初始线程个数
	init_thread_size = init_threadsize;
	cur_thread_size = init_thread_size;

	//创建线程对象
	for (int i = 0; i < init_thread_size; i++) {
		std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::thread_func, this,std::placeholders::_1));
		//线程对象需要线程函数,要把非静态成员线程函数(thread_func)给他，要bind（非静态成员函数：必须依附于一个具体的对象实例才能调用）
		int thread_id = ptr->get_id();
		threads_.emplace(thread_id,std::move(ptr));
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
void ThreadPool::thread_func(int threadid) {  //线程函数返回，线程也就结束了
	
	auto last_thread_exe_time = std::chrono::steady_clock::now();

	for (;;) {
		std::shared_ptr<Task> task;
		{	
			std::unique_lock<std::mutex> lock(task_que_mtx);

			std::cout << "TryingGet任务" << std::endl;

			//cached模式下，需要线程回收机制，当前线程空闲时间超过一定时间，就销毁线程
			//当前时间 - 上一次线程执行的时间 > 60s

			if (pool_mode == PoolMode::MODE_CACHED) {
				//每一秒钟返回一次
				while (taskque_.size() > 0) {
					if (std::cv_status::timeout == not_empty.wait_for(lock, std::chrono::seconds(1))){
						auto now = std::chrono::steady_clock::now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - last_thread_exe_time); 
						if (dur.count() >= THREAD_IDLE_TIME && cur_thread_size > init_thread_size) {
							//销毁线程
							threads_.erase(threadid);
							cur_thread_size--;
							idle_thread_size--;
							std::cout << "线程id: " << threadid << "被销毁了" << std::endl;
							return; 
						}
					}
				}
			}else {
				not_empty.wait(lock, [&]()->bool {return taskque_.size() > 0; });
			}

			
			idle_thread_size--;

			std::cout << "Get任务Success" << std::endl;

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
		last_thread_exe_time = std::chrono::steady_clock::now();
	}
}


Thread::Thread(ThreadFunc func) 
	:func_(func)
	, thread_id(++generate_id)
{}

int Thread::get_id()const {
	return thread_id;
}

Thread::~Thread() {

}

int Thread::generate_id = 0;

void Thread::start() {
	std::thread t(func_,thread_id); //线程对象t,线程函数func
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