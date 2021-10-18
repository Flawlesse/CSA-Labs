#pragma once
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <chrono>

extern long long NumTasks;

using std::thread;
using std::atomic;
using std::mutex;
using std::vector;
using namespace std::this_thread;
using namespace std::chrono;


class MutexCounter {
	size_t _counter;
	mutex _m;
public:
	MutexCounter(size_t initValue) :
		_counter(initValue)
	{}

	size_t operator++(int) {
		_m.lock();
		int tmp = _counter++;
		_m.unlock();
		return tmp;
	}
	size_t operator++() {
		_m.lock();
		int tmp = ++_counter;
		_m.unlock();
		return tmp;
	}
	size_t get() {
		return _counter;
	}
};


class AtomicCounter {
	atomic<size_t> _counter;
public:
	AtomicCounter(size_t initValue) :
		_counter(initValue)
	{}

	size_t operator++(int) {
		return _counter.fetch_add(1);
	}
	size_t operator++() {
		return _counter.fetch_add(1) + 1;
	}
	size_t get() {
		return _counter.load();
	}
};





void atomic_worker(AtomicCounter& c, vector<char>& v) {
	for (size_t i = c++; i < NumTasks; i = c++) {
		v[i] = v[i] + 1;
		sleep_for(10ns);
		//printf("\nThread ID: %u", std::hash<std::thread::id>{}(std::this_thread::get_id()));
	}
}

void runAtomicTasks(vector<char>& v, size_t NumThreads) {
	AtomicCounter c(0);
	vector<std::thread> threads(NumThreads);

	for (size_t i = 0; i < NumThreads; i++) {
		threads[i] = thread(atomic_worker, std::ref(c), std::ref(v));
	}
	for (auto& t : threads) {
		t.join();
	}
}


void mutex_worker(MutexCounter& c, vector<char>& v) {
	for (size_t i = c++; i < NumTasks; i = c++) {
		v[i] = v[i] + 1;
		sleep_for(10ns);
		//printf("\nThread ID: %u", std::hash<std::thread::id>{}(std::this_thread::get_id()));
	}
}

void runMutexTasks(vector<char>& v, size_t NumThreads) {
	MutexCounter c(0);
	vector<std::thread> threads(NumThreads);

	for (size_t i = 0; i < NumThreads; i++) {
		threads[i] = thread(mutex_worker, std::ref(c), std::ref(v));
	}
	for (auto& t : threads) {
		t.join();
	}
}