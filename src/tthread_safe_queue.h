#ifndef TTHREAD_SAFE_QUEUE_H
#define TTHREAD_SAFE_QUEUE_H

#include <queue>
#include <mutex>

///Implementation of the thread-safe queue
template <class T>
class TThreadSafeQueue {
protected:
	std::mutex Mutex;
	std::queue<T> Queue;
	size_t MaxLength;
public:
	TThreadSafeQueue(size_t max_length = 0) : MaxLength(max_length){};

	///Set max queue size
	void SetMaxLength(const size_t max_length)
	{
		Mutex.lock();
		MaxLength = max_length;
		Mutex.unlock();
	}

	///Add object to the end of queue
	bool Push(const T obj)
	{
		bool result;
		Mutex.lock();
		if (MaxLength == 0 || Queue.size() < MaxLength)
		{	
			Queue.push(obj);
			result = true;
		} else {
			result = false;
		}
		Mutex.unlock();
		return result;
	}

	///Get object from the head of the queue 
	bool Pop(T &obj)
	{
		bool result;
		Mutex.lock();
		result = !Queue.empty();
		if (result)
		{
			obj = Queue.front();
			Queue.pop();
		};
		Mutex.unlock();
		return result;
	}

	///Returns queue size
	size_t Size()
	{
		size_t result;
		Mutex.lock();
		result = Queue.size();
		Mutex.unlock();
		return result;
	}
};

#endif