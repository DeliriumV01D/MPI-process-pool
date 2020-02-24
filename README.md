This is simple MPI process pool implementation. Supporting disconnection and reconnection of the nodes.
Tasks are added and results are received in the thread-safe queues in the process #0. Tasks are distributed to all processes. 
The implementation of the process pool is contained in the files tmpi_process_pool.h и tthread_safe_queue.h. This files only needed to use MPI process pool.
Other files needed to buid the sample application.
All kinds of cooperation are welcome.
