#pragma once
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <queue>
#include <chrono>

constexpr size_t CACHE_LINE_SIZE = 64; // typically
extern size_t NumTasks2 = 4 * 1024 * 1024;

using std::atomic;
using std::condition_variable;
using std::lock_guard;
using std::mutex;
using std::queue;
using std::thread;
using std::unique_lock;
using std::vector;
using namespace std::this_thread;
using namespace std::chrono;

class BaseQueue
{
public:
	virtual void push(uint8_t val) = 0;
	virtual bool pop(uint8_t& val) = 0;
};

class DynamicQueue : BaseQueue  // works fine
{
	queue<uint8_t> container;
	mutex& m;

public:
	DynamicQueue(mutex& mt) : m(mt)
	{
	}

	virtual void push(uint8_t val)
	{
		lock_guard<mutex> lock(m);
		container.push(val);
	}

	virtual bool pop(uint8_t& val)
	{
		if (container.empty())
		{
			sleep_for(1ms);
		}
		lock_guard<mutex> lock(m);
		if (container.empty())
			return false;
		val = container.front();
		container.pop();
		return true;
	}
};




class StaticConditionQueue : BaseQueue  // works fine
{
	struct QElement
	{
		QElement* next;
		uint8_t data;
	};
	mutex& m;
	condition_variable cond;
	size_t max_size;
	size_t size;
	vector<uint8_t> _buffer;

	bool empty() { return !size; }
	bool full() { return size == max_size; }
	void fix_q() {
		for (size_t i = 1; i < size; ++i)
			_buffer[i - 1] = _buffer[i];
	}
public:
	StaticConditionQueue(mutex& mt, size_t max_size) : m(mt), size(0)
	{
		if (max_size)
			this->max_size = max_size;
		else
			this->max_size = 1;

		_buffer = vector<uint8_t>(max_size);
	}
	~StaticConditionQueue()
	{}

	virtual void push(uint8_t val)
	{
		unique_lock<mutex> lock(m);
		cond.wait(lock, [&] { return !full(); });
		_buffer[size++] = val;
		cond.notify_all();
	}

	virtual bool pop(uint8_t& val)
	{
		unique_lock<mutex> lock(m);
		bool res;
		if (res = cond.wait_for(lock, 1ms, [&] { return !empty(); }))
		{
			val = _buffer[0];
			fix_q();
			--size;
		}
		cond.notify_all();
		//if (!res) printf("NO MORE DATA\n");
		return res;
	}
};




class AtomicQueue : BaseQueue
{
private:
	struct Node
	{
		Node(uint8_t val) : _value(val), _next(nullptr) {}
		uint8_t _value;
		atomic<Node*> _next; // shared between all
		// each Node is padded to be on exactly ONE cache line
		char pad[CACHE_LINE_SIZE - sizeof(uint8_t) - sizeof(atomic<Node*>)];
	};
	char pad0[CACHE_LINE_SIZE];

	Node* _head; // _head is not shared
	char pad1[CACHE_LINE_SIZE - sizeof(Node*)];

	atomic<bool>  _consumerLock;
	char pad2[CACHE_LINE_SIZE - sizeof(atomic<bool>)];

	Node* _tail; // _tail is not shared
	char pad3[CACHE_LINE_SIZE - sizeof(Node*)];

	atomic<bool>  _producerLock;
	char pad4[CACHE_LINE_SIZE - sizeof(atomic<bool>)];
public:
	AtomicQueue() {
		_head = _tail = new Node(0); // head points to pseudo-element before actual head
	}
	~AtomicQueue()
	{
		while (_head != nullptr)
		{
			Node* tmp = _head;
			_head = _head->_next;
			delete tmp;
		}
	}

	// producers only use _tail pointer
	void push(const uint8_t val)
	{
		Node* tmp = new Node(val);
		while (_producerLock.exchange(true)); // producer mutex lock
		_tail->_next = tmp;
		_tail = tmp;
		_producerLock = false; // producer mutex unlock
	}
	// consumers only use _head pointer
	bool pop(uint8_t& val)
	{
		while (_consumerLock.exchange(true)); // consumer mutex lock
		Node* head_to_delete = _head;
		Node* actual_head = _head->_next;
		if (!actual_head) {
			sleep_for(1ms);
			head_to_delete = _head;
			actual_head = _head->_next;
		}
		if (actual_head) {
			val = actual_head->_value;
			actual_head->_value = 0;
			_head = actual_head;
			_consumerLock = false; // consumer lock unlock
			delete head_to_delete;
			return true;
		}
		_consumerLock = false;
		return false;
	}
};



class Producer
{
	uint64_t numTasks;
	BaseQueue* _queue;

public:
	Producer(BaseQueue* q, uint64_t nt) : numTasks(nt), _queue(q)
	{
	}

	void operator()()
	{
		int nt = numTasks;
		while (numTasks--)
		{
			_queue->push(1);
			//printf("TASKS REMAIN = %u   PRODUCER NUM TASKS = %u\n", (int)numTasks, nt - (int)numTasks);
		}
	}
};

class Consumer
{
	size_t localSum = 0;
	BaseQueue* _queue;

public:
	Consumer(BaseQueue* q) : _queue(q)
	{
	}

	void operator()()
	{
		uint8_t val;
		while (_queue->pop(val))
		{
			localSum = localSum + val;
			//printf("VAL = %u CONSUMED\n", val);
		}
	}

	size_t get_local_sum() const { return localSum; }
};



void runTasks(uint64_t& sum, size_t numConsumers, size_t numProducers, BaseQueue* q) {
	vector<thread> consumerThreads(numConsumers);
	vector<Consumer> consumers(numConsumers, Consumer(q));

	vector<thread> producerThreads(numProducers);
	vector<Producer> producers(numProducers, Producer(q, NumTasks2 / numProducers));

	for (size_t i = 0; i < producerThreads.size(); i++)
	{
		producerThreads[i] = thread(producers[i]);
	}
	for (size_t i = 0; i < consumerThreads.size(); i++)
	{
		consumerThreads[i] = thread(std::ref(consumers[i]));
	}
	for (auto& t : producerThreads) t.join();
	for (auto& t : consumerThreads) t.join();

	sum = 0;
	for (int i = 0; i < numConsumers; i++)
	{
		sum += consumers[i].get_local_sum();
		printf("Consumer %d localSum=%u\n", i, consumers[i].get_local_sum());
	}
}

void runDynamicQueue(uint64_t& sum, size_t numConsumers, size_t numProducers)
{
	mutex m;
	DynamicQueue q(m);
	runTasks(sum, numConsumers, numProducers, (BaseQueue*)&q);
}

void runStaticQueue(uint64_t& sum, size_t numConsumers, size_t numProducers, size_t qsize) {
	mutex m;
	StaticConditionQueue q(m, qsize);
	runTasks(sum, numConsumers, numProducers, (BaseQueue*)&q);
}

void runAtomicQueue(uint64_t& sum, size_t numConsumers, size_t numProducers) {
	AtomicQueue q;
	printf("Tasks started.\n");
	runTasks(sum, numConsumers, numProducers, (BaseQueue*)&q);
	printf("Tasks finished.\n");
}