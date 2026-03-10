#include "../include/threadpool.h"

#include<iostream>
#include<chrono>
#include<thread>


/*

*/

class MyTask : public Task {
public:
	Any run() override {
		std::this_thread::sleep_for(std::chrono::seconds(1));
		std::cout << "mytid: " << std::this_thread::get_id() << std::endl;
		int sum = 0;
		return sum;
	}
};

int main() {

	ThreadPool pool;
	pool.start(4);

	Result res = pool.submit_task(std::make_shared<MyTask>());
	
	int sum = res.get().cast_<int>();

	pool.submit_task(std::make_shared<MyTask>());
	pool.submit_task(std::make_shared<MyTask>());
	pool.submit_task(std::make_shared<MyTask>());
	pool.submit_task(std::make_shared<MyTask>());
	pool.submit_task(std::make_shared<MyTask>());

	getchar();
	//std::this_thread::sleep_for(std::chrono::seconds(5));
}