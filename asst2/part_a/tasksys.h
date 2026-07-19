#ifndef _TASKSYS_H
#define _TASKSYS_H
#include "thread"

#include "itasksys.h"

/*
 * TaskSystemSerial: This class is the student's implementation of a
 * serial task execution engine.  See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemSerial : public ITaskSystem {
public:
    TaskSystemSerial(int num_threads);

    ~TaskSystemSerial();

    const char *name();

    void run(IRunnable *runnable, int num_total_tasks);

    TaskID runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                            const std::vector<TaskID> &deps);

    void sync();
};

/*
 * TaskSystemParallelSpawn: This class is the student's implementation of a
 * parallel task execution engine that spawns threads in every run()
 * call.  See definition of ITaskSystem in itasksys.h for documentation
 * of the ITaskSystem interface.
 */
class TaskSystemParallelSpawn : public ITaskSystem {
private:
    int num_threads_;

public:
    TaskSystemParallelSpawn(int num_threads);

    ~TaskSystemParallelSpawn();

    const char *name();

    void run(IRunnable *runnable, int num_total_tasks);

    TaskID runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                            const std::vector<TaskID> &deps);

    void sync();
};

/*
 * TaskSystemParallelThreadPoolSpinning: This class is the student's
 * implementation of a parallel task execution engine that uses a
 * thread pool. See definition of ITaskSystem in itasksys.h for
 * documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSpinning : public ITaskSystem {
private:
    int num_threads_;
    std::vector<std::thread> workers_;
    std::atomic<bool> shutdown_;
    std::atomic<int> next_task_;
    std::atomic<int> completed_tasks_;

    IRunnable *current_runnable_;
    int current_num_tasks_;

public:
    TaskSystemParallelThreadPoolSpinning(int num_threads);

    ~TaskSystemParallelThreadPoolSpinning();

    const char *name();

    void sync();

    void run(IRunnable *runnable, int num_total_tasks);

    TaskID runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                            const std::vector<TaskID> &deps);
};

/*
 * TaskSystemParallelThreadPoolSleeping: This class is the student's
 * optimized implementation of a parallel task execution engine that uses
 * a thread pool. See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSleeping : public ITaskSystem {
public:
    TaskSystemParallelThreadPoolSleeping(int num_threads);

    ~TaskSystemParallelThreadPoolSleeping();

    const char *name();

    void sync();

    void run(IRunnable *runnable, int num_total_tasks);

    TaskID runAsyncWithDeps(
        IRunnable *runnable,
        int num_total_tasks,
        const std::vector<TaskID> &deps
    );
private:
    int num_threads_;
    std::vector<std::thread> workers_;
    std::mutex_;
    std::conditions_variable work_available_cv_;
    std::conditions_variable all_task_done_cv;
    
};

#endif
