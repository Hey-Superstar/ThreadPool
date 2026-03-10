#include "../include/threadpool.h"

#include<iostream>
#include<chrono>
#include<thread>


/*

*/

class MyTask : public Task {
public:
	void run() {
		std::this_thread::sleep_for(std::chrono::seconds(1));
		std::cout << "mytid: " << std::this_thread::get_id() << std::endl;
	}
};

int main() {

	ThreadPool pool;
	pool.start(4);

	pool.submit_task(std::make_shared<MyTask>());
	pool.submit_task(std::make_shared<MyTask>());
	pool.submit_task(std::make_shared<MyTask>());
	pool.submit_task(std::make_shared<MyTask>());
	pool.submit_task(std::make_shared<MyTask>());

	getchar();
	//std::this_thread::sleep_for(std::chrono::seconds(5));
}