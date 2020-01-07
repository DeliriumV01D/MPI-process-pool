#pragma once

#include <mpi.h>

#include <atomic>
#include <functional>

#include "tthread_safe_queue.h"

//Example of the command
//"C:\MPICH2\bin\mpiexec.exe" -localroot -wdir C:\ExampleFolder -hosts 4 Host1 8 Host2 6 Host3 4 Host4 4 -noprompt C:\ExampleFolder\example.exe C:\ExampleFolder\params.conf

///Interface of the task class. It must support writing/reading from buffer and executed stamp
class IMPITask {
public:
	IMPITask(){};
	virtual ~IMPITask(){};

	virtual size_t GetBufferSize(){return 0;};
	virtual unsigned char * ToBuffer(){return nullptr;};
	virtual void FromBuffer(unsigned char * buffer){};
};

///Interface of the result class. It must support writing/reading from buffer, start timestamp and process rank
class IMPIResult {
public:
	IMPIResult(){};
	virtual ~IMPIResult(){};

	virtual size_t GetBufferSize(){return 0;};
	virtual unsigned char * ToBuffer(){return nullptr;};
	virtual void FromBuffer(unsigned char * buffer){};
};

///Process states
enum TProcessState{
	IDLE,
	BUSY,
	UNAVAILABLE
};

///Simple MPI process pool implementation. Supporting disconnection and reconnection of the nodes
template <typename TTask, typename TResult>
class TMPIProcessPool {
protected:
	std::atomic<bool>	Stopped,
										Finished;
	int MPIRank,	
			MPISize;
	std::vector<TProcessState> ProcessStates;		//No mutex required. There is only one thread working with this array
	std::function<TResult(TTask *)> ResultFunc;
	TThreadSafeQueue <IMPITask *> TaskQueue;
	TThreadSafeQueue <IMPIResult *> ResultQueue;
public:
	TMPIProcessPool(
		std::function<TResult (TTask*)> result_func,
		const size_t max_task_queue_size
	){
		ResultFunc = result_func;
		MPI::Init();
		TaskQueue.SetMaxLength(max_task_queue_size);
	}
				 
	~TMPIProcessPool()
	{
		while (!Finished)
			std::this_thread::sleep_for(std::chrono::microseconds(100));
		MPI::Finalize();
	}

	void Start()
	{
		//Initialization
		MPI_Comm_size(MPI_COMM_WORLD, &MPISize);
		MPI_Comm_rank(MPI_COMM_WORLD, &MPIRank);	
		Finished = false;
		Stopped = false;
		ProcessStates.resize(MPISize);
		for (auto it : ProcessStates)
			it = IDLE;

		if (MPIRank == 0)
		{
			//!!!Вынести из метода, можно оставить лямбдой
			auto f = [this]()
			{
				//Processing loop
				std::cout<<MPIRank<<" start..."<<std::endl;
				size_t buf_size = 0;
				MPI_Status status;
				IMPITask * task;
				do {
					//std::this_thread::sleep_for(std::chrono::microseconds(100)); ///!!!Переделать через conditions variable
					//Distribite tasks and receive results
					for (int i = 1/*!=0*/; i < ProcessStates.size(); i++)
						if (ProcessStates[i] == IDLE)
						{
							if (TaskQueue.Pop(task))
							{
								//Send the task, first send a packet size
								buf_size = task->GetBufferSize();
								MPI_Send(&buf_size, sizeof(size_t), MPI_CHAR, i, 0, MPI_COMM_WORLD);
								MPI_Send(task->ToBuffer(), (int)buf_size, MPI_CHAR, i, 0, MPI_COMM_WORLD);
								delete task;	//don't forget to free memory
								ProcessStates[i] = BUSY;
							} else {
								break;
							}
						}

					//All solutions are received = no tasks and no busy processes - stop calculations
					//!!!Не красиво, что надо обходить массив состояний, лучше завести отдельный set для занятых процессов, например
					bool NoBusyProcesses = true;
					for (int i = 1/*!=0*/; i < ProcessStates.size(); i++)
						if (ProcessStates[i] == BUSY)
						{
							NoBusyProcesses = false;
							break;
						}	 
					if (TaskQueue.Size() == 0 && NoBusyProcesses)
						Stopped = true;

					if (!Stopped && !NoBusyProcesses)
					{
						//If one package received from process(package size), wait for another package from this process
						//!!!В случае если узел отключется между этими двумя событиями программа зависнет
						//!!!Чтобы не загружать процессор в этом месте на 100% можно сделать через асинхронные MPI_IRecv?
						MPI_Recv(&buf_size, sizeof(size_t), MPI_CHAR, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
						std::vector<unsigned char> buffer(buf_size);
						MPI_Recv(buffer.data(), (int)buf_size, MPI_CHAR, status.MPI_SOURCE, 0, MPI_COMM_WORLD, &status);
						ProcessStates[status.MPI_SOURCE] = IDLE;
						IMPIResult * result = new TResult;
						result->FromBuffer(buffer.data());
						ResultQueue.Push(result);
					}		
				}	while (!Stopped);

				//if flag stopped is on - send a stop command to the work processes
				//Finalization
				size_t sz = 0;
				for (int i = 1; i < MPISize; i++)
					MPI_Send(&sz, sizeof(size_t), MPI_CHAR, i, 0, MPI_COMM_WORLD);
				Finished = true;
				std::cout<<MPIRank<<" done..."<<std::endl;
				std::cout<<"reults: "<<ResultQueue.Size()<<std::endl;
			};	 //lambda
			std::thread th(f);
			th.detach();

		}	else {	//(mpi_rank != 0)
			//Receive task or stop command(0 - size package), receive a package size first
			size_t buf_size = 0;
			MPI_Status status;
			while (!Stopped)
			{
				MPI_Recv(&buf_size, sizeof(size_t), MPI_CHAR, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
				if (buf_size != 0)
				{
					std::vector<unsigned char> buffer(buf_size);
					MPI_Recv(buffer.data(), (int)buf_size, MPI_CHAR, status.MPI_SOURCE, 0, MPI_COMM_WORLD, &status);
					//Solving the problem, geting a result
					TResult result;
					TTask task;
					task.FromBuffer(buffer.data());
					result = ResultFunc(&task);
					//Send a packege. First send pakege size.
					buf_size = result.GetBufferSize();
					MPI_Send(&buf_size, sizeof(size_t), MPI_CHAR, status.MPI_SOURCE, 0, MPI_COMM_WORLD);
					MPI_Send(result.ToBuffer(), (int)result.GetBufferSize(), MPI_CHAR, status.MPI_SOURCE, 0, MPI_COMM_WORLD);
				} else {
					Stopped = true;
				}
			}
			Finished = true;
		}
	}

	void Stop()
	{
		Stopped = true;
	}

	bool IsFinished()
	{
		return Finished;
	}

	size_t GetTaskQueueLength()
	{
		return TaskQueue.Size();
	}

	bool AddTask(IMPITask * task)
	{
		return TaskQueue.Push(task);
	}

	size_t GetResultQueueLength()
	{
		return ResultQueue.Size();
	}

	bool GetResult(IMPIResult ** result)
	{
		return ResultQueue.Pop(*result);
	}
};

//                    std::function<void()> task;
//                    {
//                        std::unique_lock<std::mutex> lock(this->queue_mutex);
//                        this->condition.wait(lock,
//                            [this]{ return this->stop || !this->tasks.empty(); });
//                        if(this->stop && this->tasks.empty())
//                            return;
//Переделать Queue.pop через:
//                        task = std::move(this->tasks.front());
//                        this->tasks.pop();
//                    }