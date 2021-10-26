#pragma once
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <queue>
#include <chrono>

constexpr size_t CACHE_LINE_SIZE = 64; // typically
extern uint64_t NumTasks2 = 1024;

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
	QElement* front, * back, * _tail;

	bool empty() { return !size; }
	bool full() { return size == max_size; }

	void fix_q()
	{
		// just rebind front element container to
		// the tail of the whole queue
		auto tmp = front;
		front = front->next;
		tmp->next = front;
		_tail->next = tmp;
		_tail = _tail->next;
	}

public:
	StaticConditionQueue(mutex& mt, size_t max_size) : m(mt), size(0)
	{
		if (max_size)
			this->max_size = max_size;
		else
			this->max_size = 1;

		front = new QElement{ back, 0 };
		back = front;
		back->next = front;
		QElement* prev = front;
		for (size_t i = 1; i < max_size; i++)
		{
			QElement* elem = new QElement{ nullptr, 0 };
			prev->next = elem;
			prev = prev->next;
		}
		_tail = prev;
		_tail->next = front;
	}
	~StaticConditionQueue()
	{
		auto curr = front;
		while (curr != _tail)
		{
			auto next = curr->next;
			delete curr;
			curr = next;
		}
		delete _tail;
	}

	virtual void push(uint8_t val)
	{
		unique_lock<mutex> lock(m);
		cond.wait(lock, [&] { return !full(); });
		back->data = val;
		back = back->next;
		++size;
		//printf("PUSH --> SIZE = %u\n", size);
		lock.unlock();
		cond.notify_all();
	}

	virtual bool pop(uint8_t& val)
	{
		unique_lock<mutex> lock(m);
		bool res;
		if (res = cond.wait_for(lock, 1ms, [&] { return !empty(); }))
		{
			val = front->data;
			fix_q();
			--size;
			//printf("POP  --> SIZE = %u\n", size);
			//printf("popping... Size = %u\n", this->size);
		}
		lock.unlock();
		cond.notify_all();
		//if (!res) printf("We did not wait long enough...\n");
		return res;
	}
};




class AtomicQueue {
	struct Container {
		uint8_t* _inner;
		Container(size_t max_size) {
			_inner = (uint8_t*)calloc(max_size, 1);
		}
		~Container() {
			free(_inner);
		}
		void inner_push(uint8_t val, size_t i) {
			_inner[i] = val;
		}
		void inner_pop(uint8_t& val, const size_t &max_size) {
			val = _inner[0];
			memcpy(_inner, _inner + 1, max_size - 1);
		}
	};

	const size_t max_size;
	atomic<size_t> size;
	atomic<Container*> container;

	condition_variable cond;
	mutex m;
	
	bool empty() { return size == 0; }
	bool full() { return size == max_size; }
public:
	AtomicQueue(size_t qsize):
	max_size(qsize), size(0)
	{
		container = new Container(qsize);
	}
	~AtomicQueue() {
		delete container;
	}
	
	virtual void push(uint8_t val) {
		unique_lock<mutex> lock(m);
		cond.wait(lock, [&] {return !full(); });
		(*container).inner_push(val, size);
		++size;
		cond.notify_all();
	}

	virtual bool pop(uint8_t& val) {
		unique_lock<mutex> lock(m);
		bool res;
		if (res = cond.wait_for(lock, 1ms, [&] { return !empty(); })) {
			(*container).inner_pop(val, max_size);
			--size;
		}
		cond.notify_all();
		return res;
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
		while (numTasks--)
		{
			_queue->push(1);
			//printf("TASKS REMAIN = %u   PRODUCER NUM TASKS = %u\n", numTasks, 16 - numTasks);
		}
	}
};

class Consumer
{
	uint64_t localSum = 0;
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
			//printf("VAL = %u    CONSUMER LOCAL SUM = %u\n", val, localSum);
		}
	}

	uint64_t get_local_sum() const { return localSum; }
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
		printf("Consumer %u localSum=%u\n", i, consumers[i].get_local_sum());
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

void runAtomicQueue(uint64_t& sum, size_t numConsumers, size_t numProducers, size_t qsize) {
	AtomicQueue q(qsize);
	runTasks(sum, numConsumers, numProducers, (BaseQueue*)&q);
}



//class AtomicQueue : BaseQueue
//{
//private:
//	struct Node
//	{
//		Node(uint8_t val) : _value(val), _next(nullptr)
//		{
//		}
//		uint8_t _value;
//		atomic<Node*> _next; // shared between all
//		// each Node is padded to be on exactly ONE cache line
//		char pad[CACHE_LINE_SIZE - sizeof(uint8_t) - sizeof(atomic<Node*>)];
//	};
//	char pad0[CACHE_LINE_SIZE];
//
//	Node* _head; // _head is not shared
//	char pad1[CACHE_LINE_SIZE - sizeof(Node*)];
//
//	 mutex _consumerLock; // shared between consumers (one at a time)
//	 char pad2[CACHE_LINE_SIZE - sizeof(mutex)];
//	/*atomic<bool>  _consumerLock;
//	char pad2[CACHE_LINE_SIZE - sizeof(atomic<bool>)];*/
//
//	Node* _tail; // _tail is not shared
//	char pad3[CACHE_LINE_SIZE - sizeof(Node*)];
//
//	 mutex _producerLock; // shared between producers (one at a time)
//	 char pad4[CACHE_LINE_SIZE - sizeof(mutex)];
//	/*atomic<bool>  _producerLock;
//	char pad4[CACHE_LINE_SIZE - sizeof(atomic<bool>)];*/
//
//	atomic<size_t> size; // shared between all
//	char pad5[CACHE_LINE_SIZE - sizeof(atomic<size_t>)];
//
//	size_t max_size;
//	char pad6[CACHE_LINE_SIZE - sizeof(size_t)];
//
//	condition_variable cond;
//	char pad7[CACHE_LINE_SIZE - sizeof(condition_variable)];
//
//	bool empty() { return size == 0; }
//	bool full() { return size == max_size; }
//public:
//	AtomicQueue(size_t qsize):
//	size(0)
//	{
//		if (qsize)
//			max_size = qsize;
//		else
//			max_size = 1;
//		_head = _tail = new Node(0); // head points to pseudo-element before actual head
//	}
//	~AtomicQueue()
//	{
//		while (_head != nullptr)
//		{
//			Node* tmp = _head;
//			_head = _head->_next;
//			delete tmp;
//		}
//	}
//
//	// producers only use _tail pointer
//	void push(const uint8_t val)
//	{
//		Node* tmp = new Node(val);
//		//while (_producerLock.exchange(true)) ; // producer mutex lock
//		unique_lock<mutex> prodLock(_producerLock);
//		/*while (full()) { 
//			printf("waiting in push... SIZE = %u\n", size.load());
//			sleep_for(10ns);
//		}*/
//		cond.wait(prodLock, [&] { return !full(); });
//		//printf("Pushing...\n");
//		_tail->_next = tmp;
//		_tail = tmp;
//		cond.notify_all();
//		prodLock.unlock();
//		//_producerLock = false; // producer mutex unlock
//		++size;
//		//printf("SIZE after PUSH (non-critical region) = %u\n", size.load());
//		//printf("Size = %u\n", size.load());
//	}
//	// consumers only use _head pointer
//	bool pop(uint8_t& val)
//	{
//		Node* head_to_delete = _head;
//		Node* actual_head = _head->_next;
//		bool res;
//		////while (_consumerLock.exchange(true)) ; // consumer mutex lock
//		//if (res = cond.wait_for(consLock, 1ms, [&] { return !empty(); })) {
//		//	head_to_delete = _head;
//		//	actual_head = _head->_next;
//		//	//printf("Popping...\n");
//		//	val = actual_head->_value;
//		//	actual_head->_value = 0;
//		//	_head = actual_head;
//		//	//_consumerLock = false; // consumer mutex unlock
//		//	delete head_to_delete;
//		//}
//		//consLock.unlock();
//		//if (res) {
//		//	--size;
//		//	//printf("Size = %u\n", size.load());
//		//}
//		//return res;
//		//while (_consumerLock.exchange(true)); // consumer mutex lock
//		
//		unique_lock<mutex> consLock(_consumerLock);
//		
//		//if (this->empty()) {
//		//	//printf("Wow, it's firstly empty in POP! SIZE = %u\n", size.load());
//		//	sleep_for(5ms);
//		//	head_to_delete = _head;
//		//	actual_head = _head->_next;
//		//}
//
//		if (res = cond.wait_for(consLock, 5ms, [&] { return !empty(); }))
//		{
//			head_to_delete = _head;
//			actual_head = _head->_next;
//			//printf("Now it's not empty in POP! SIZE = %u\n", size.load());
//			val = actual_head->_value;
//			actual_head->_value = 0;
//			_head = actual_head;
//			//_consumerLock = false; // consumer mutex unlock
//			cond.notify_all();
//			consLock.unlock(); 
//			--size;
//			//printf("SIZE after POP (non-critical region) = %u\n", size.load());
//			//_consumerLock = false; // consumer mutex unlock
//			delete head_to_delete;
//			return true;
//		}
//		//_consumerLock = false; // consumer mutex unlock
//		printf("OK NO ELEMENTS REMAINING\n");
//		//sleep_for(3s);
//		return false;
//	}
//};