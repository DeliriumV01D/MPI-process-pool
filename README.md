This is simple MPI process pool implementation. Supporting disconnection and reconnection of the nodes.
Tasks are added and results are received in the thread-safe queues in the process #0. Tasks are distributed to all processes. 