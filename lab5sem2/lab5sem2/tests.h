#pragma once
#include <thread>
#include <iostream>
#include <mutex>
#include <vector>
#include <atomic>
#include <chrono>
#include "task1.h"
#include "task2.h"

using std::cout;
using std::cin;
using std::endl;
using std::vector;
using namespace std::chrono;

extern size_t NumTasks1;
extern uint64_t NumTasks2;

const bool
enableDynamicTest = false,
enableStaticTest = false,
enableAtomicTest = true,
enableCounterTests = false;

void CounterTests() {
	// TASK 1
	if (!enableCounterTests) return;

	vector<char> v(NumTasks1);

	cout << "-------------LINEAR COUNTER---------------" << endl;
	auto start = steady_clock().now();
	for (size_t i = 0; i < NumTasks1; i++) {
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
}


void QueuesTests() {
	// TASK 2
	uint64_t sum = 0;

	cout << endl << "-----------------TASK 2------------------" << endl << endl;
	if (enableStaticTest) {
		cout << "-----------------CONDITION QUEUE------------------" << endl;
		for (size_t consumers : {1, 2, 4}) {
			for (size_t producers : {1, 2, 4}) {
				for (size_t qsize : {1, 4, 16}) {
					auto start = steady_clock().now();
					runStaticQueue(sum, consumers, producers, qsize);
					auto finish = steady_clock().now();
					cout << "NumConsumers: " << consumers << endl;
					cout << "NumProducers: " << producers << endl;
					cout << "Queue size: " << qsize << endl;
					cout << "Resulting sum: " << sum << endl;
					cout << "Time elapsed: " << duration_cast<milliseconds>(finish - start).count() << "ms" << endl << endl;
				}
			}
		}
	}

	if (enableDynamicTest) {
		cout << "-----------------DYNAMIC QUEUE------------------" << endl;
		for (size_t consumers : {1, 2, 4}) {
			for (size_t producers : {1, 2, 4}) {
				auto start = steady_clock().now();
				runDynamicQueue(sum, consumers, producers);
				auto finish = steady_clock().now();
				cout << "NumConsumers: " << consumers << endl;
				cout << "NumProducers: " << producers << endl;
				cout << "Resulting sum: " << sum << endl;
				cout << "Time elapsed: " << duration_cast<milliseconds>(finish - start).count() << "ms" << endl << endl;
			}
		}
	}

	if (enableAtomicTest) {
		cout << "-----------------ATOMIC QUEUE------------------" << endl;
		for (size_t consumers : {1, 2, 4}) {
			for (size_t producers : {1, 2, 4}) {
					auto start = steady_clock().now();
					runAtomicQueue(sum, consumers, producers); 
					auto finish = steady_clock().now();
					cout << "NumConsumers: " << consumers << endl;
					cout << "NumProducers: " << producers << endl;
					cout << "Resulting sum: " << sum << endl;
					cout << "Time elapsed: " << duration_cast<milliseconds>(finish - start).count() << "ms" << endl << endl;
			}
		}
	}
}