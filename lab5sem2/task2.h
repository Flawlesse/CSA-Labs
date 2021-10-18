#pragma once
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <queue>
#include <chrono>

extern long long NumTasks;

using std::thread;
using std::atomic;
using std::mutex;
using std::vector;
using std::queue;
using namespace std::this_thread;
using namespace std::chrono;

class DynamicQueue {
	queue<uint8_t> container;
	mutex m;
public:
	DynamicQueue(){}
	
	bool empty() {
		return container.empty();
	}

	void push(uint8_t val) {
		m.lock();
		container.push(val);
		m.unlock();
	}

	bool pop(uint8_t& val) {
		while (container.empty()) {
			sleep_for(1ms);
		}
		val = container.front();
	}
};

class FixedConditionQueue {

};