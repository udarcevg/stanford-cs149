#ifndef _TASKSYS_H
#define _TASKSYS_H

#include "itasksys.h"
#include <memory>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

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
public:
    TaskSystemParallelThreadPoolSpinning(int num_threads);

    ~TaskSystemParallelThreadPoolSpinning();

    const char *name();

    void run(IRunnable *runnable, int num_total_tasks);

    TaskID runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                            const std::vector<TaskID> &deps);

    void sync();
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

    void sync();

    const char *name();

    void run(IRunnable *runnable, int num_total_tasks);

    TaskID runAsyncWithDeps(
        IRunnable *runnable,
        int num_total_tasks,
        const std::vector<TaskID> &deps
    );

private:
    struct BulkTask {
        TaskID id;
        IRunnable *runnable;

        int num_total_tasks;
        int next_task;
        int completed_tasks;
        int remaining_dependencies;
        bool completed;
        std::vector<TaskID> dependents;
    };

    void workerLoop();

    void completeLaunchLocked(TaskID launch_id);

    int num_threads_;
    std::vector<std::thread> workers_;
    std::mutex mutex_;
    std::condition_variable work_available_;
    std::condition_variable all_work_done_;
    bool shutdown_;
    TaskID next_launch_id_;
    int total_launches_;
    int completed_launches_;
    std::vector<std::unique_ptr<BulkTask> > launches_;
    std::queue<TaskID> ready_launches_;
};

#endif
