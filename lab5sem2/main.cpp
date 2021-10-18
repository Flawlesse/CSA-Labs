#include <thread>
#include <iostream>
#include <mutex>
#include <vector>
#include <atomic>
#include <chrono>
#include "task1.h"

extern long long NumTasks = 1024*1024;

using std::cout;
using std::cin;
using std::endl;
using std::vector;
using namespace std::chrono;


void clear(vector<char>& v) {
	for (auto& c : v)
		c = 0;
}


void displayResult(const vector<char>& v) {
	cout << "\nV: ";
	for (auto &c : v)
		cout << (int)c << ' ';
	cout << endl;
}

bool is_all_ones(const vector<char>& v) {
	for (auto& c : v)
		if (c != 1)
			return false;
	return true;
}

int main() {
	// TASK 1
	vector<char> v(NumTasks);
	
	cout << "-------------LINEAR COUNTER---------------" << endl;
	auto start = steady_clock().now();
	for (size_t i = 0; i < NumTasks; i++) {
		v[i] = v[i] + 1;
		std::this_thread::sleep_for(10ns);
	}
	auto finish = steady_clock().now();
	cout << "\nFor single thread (no mutex or atomic): " << duration_cast<milliseconds>(finish - start).count() << "ms" << endl;
	cout << endl;

	cout << "-------------ATOMIC COUNTER---------------" << endl;
	for (size_t nt : { 4, 8, 16, 32 }) {
		clear(v);
		auto start = steady_clock().now();
		runAtomicTasks(v, nt);
		auto finish = steady_clock().now();
		cout << "\nFor " << nt << " threads: " << duration_cast<milliseconds>(finish - start).count() << "ms" << endl;
		//displayResult(v);
		cout << "Is all ones: " << std::boolalpha << is_all_ones(v) << endl;
	}

	cout << endl;
	cout << "-------------MUTEX COUNTER---------------" << endl;
	for (size_t nt : { 4, 8, 16, 32 }) {
		clear(v);
		auto start = steady_clock().now();
		runMutexTasks(v, nt);
		auto finish = steady_clock().now();
		cout << "\nFor " << nt << " threads: " << duration_cast<milliseconds>(finish - start).count() << "ms" << endl;
		//displayResult(v);
		cout << "Is all ones: " << std::boolalpha << is_all_ones(v) << endl;
	}


	// TASK 2


	return 0;
}