#include <iostream>
#include "tmpi_process_pool.h"

//Simple test of the MPI process pool with TestTask, TestResult	and f

class TestTask : public IMPITask {
protected:
	int ID;
public:
	TestTask(const int id = 0) : IMPITask()
	{
		ID = id;
	}

	int GetID()
	{
		return ID;
	}

	size_t GetBufferSize()
	{
		return sizeof(int);
	}

	unsigned char * ToBuffer()
	{
		return (unsigned char *)&ID;
	}

	void FromBuffer(unsigned char * buffer)
	{
		ID = *(int *)buffer;
	};
};

class TestResult : public IMPIResult {
	int ID;
public:
	TestResult(const int id = 0) : IMPIResult()
	{
		ID = id;
	}
		
	int GetID()
	{
		return ID;
	}

	size_t GetBufferSize()
	{
		return sizeof(int);
	}

	unsigned char * ToBuffer()
	{
		return (unsigned char *)&ID;
	}

	void FromBuffer(unsigned char * buffer)
	{
		ID = *(int *)buffer;
	};
};

int main(int argc, char *argv[])
{
	//Process pool initializing with lambda function that gets task object and returns result object 
	auto f = [](TestTask *task)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		return TestResult(task->GetID());
	};

	TMPIProcessPool <TestTask, TestResult> mpi_process_pool(f, 100);

	mpi_process_pool.Start();
	
	if (mpi_process_pool.GetMPIRank() == 0)
	{
		for (int i = 0; i < 35; i++)
			bool b = mpi_process_pool.AddTask(new TestTask(i));

		//It’s not necessary to wait until the end of the work, you can get the results as they received
		while (!mpi_process_pool.IsFinished())
			std::this_thread::sleep_for(std::chrono::milliseconds(1));

		IMPIResult ** result = new IMPIResult *;
		while (mpi_process_pool.GetResult(result))
		{
			std::cout<<"RESULT: "<<((TestResult*)(*result))->GetID()<<std::endl;
			delete (*result);
		}
		delete result;
	}
	//process #0 will stop when all answers are received, other processes will stop by command
	//mpi_process_pool.Stop();
}
