#include "tasksys.h"
#include "thread"
#include <atomic>
#include <vector>

IRunnable::~IRunnable() {
}

ITaskSystem::ITaskSystem(int num_threads) {
}

ITaskSystem::~ITaskSystem() {
}

/*
 * ================================================================
 * Serial task system implementation
 * ================================================================
 */

const char *TaskSystemSerial::name() {
    return "Serial";
}

TaskSystemSerial::TaskSystemSerial(int num_threads) : ITaskSystem(num_threads) {
}

TaskSystemSerial::~TaskSystemSerial() {
}

void TaskSystemSerial::run(IRunnable *runnable, int num_total_tasks) {
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                                          const std::vector<TaskID> &deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemSerial::sync() {
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Task System Implementation
 * ================================================================
 */

const char *TaskSystemParallelSpawn::name() {
    return "Parallel + Always Spawn";
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads) : ITaskSystem(num_threads),
                                                                    num_threads_(num_threads) {
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {
}

void TaskSystemParallelSpawn::run(
    IRunnable *runnable,
    int num_total_tasks
) {
    if (num_total_tasks <= 0) {
        return;
    }

    std::atomic<int> next_task(0);
    std::vector<std::thread> workers;

    int worker_count = std::min(num_threads_, num_total_tasks);

    for (int i = 0; i < worker_count; i++) {
        workers.emplace_back([&]() {
            while (true) {
                int task_id = next_task.fetch_add(1);

                if (task_id >= num_total_tasks) {
                    break;
                }

                runnable->runTask(task_id, num_total_tasks);
            }
        });
    }

    for (std::thread &worker: workers) {
        worker.join();
    }
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                                                 const std::vector<TaskID> &deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemParallelSpawn::sync() {
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Spinning Task System Implementation
 * ================================================================
 */

const char *TaskSystemParallelThreadPoolSpinning::name() {
    return "Parallel + Thread Pool + Spin";
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads) : ITaskSystem(num_threads),
    num_threads_(num_threads),
    shutdown_(false),
    next_task_(0),
    completed_tasks_(0),
    current_runnable_(nullptr),
    current_num_tasks_(0) {
    for (int i = 0; i < num_threads_; i++) {
        workers_.emplace_back([this]() {
            while (!shutdown_.load()) {
                IRunnable *runnable = current_runnable_;

                if (runnable == nullptr) {
                    continue;
                }

                int task_id = next_task_.fetch_add(1);
                if (task_id >= current_num_tasks_) {
                    continue;
                }
                runnable->runTask(task_id, current_num_tasks_);
                completed_tasks_.fetch_add(1);
            }
        });
    }
}


TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {
    shutdown_.store(true);
    for (std::thread &worker: workers_) {
        worker.join();
    }
}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable *runnable, int num_total_tasks) {
    if (num_total_tasks <= 0) {
        return;
    }
    next_task_.store(0);
    completed_tasks_.store(0);

    current_num_tasks_ = num_total_tasks;
    current_runnable_ = runnable;
    while (completed_tasks_.load() < num_total_tasks) {
        // Yield execution to prevent the main thread from burning 100% CPU while spinning
        std::this_thread::yield();
    }
    current_runnable_ = nullptr;
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                                                              const std::vector<TaskID> &deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

const char *TaskSystemParallelThreadPoolSleeping::name() {
    return "Parallel + Thread Pool + Sleep";
}


TaskSystemParallelThreadPoolSleeping::
TaskSystemParallelThreadPoolSleeping(int num_threads)
    : ITaskSystem(num_threads),
      num_threads_(num_threads),
      shutdown_(false),
      work_available_(false),
      current_runnable_(nullptr),
      current_num_tasks_(0),
      next_task_(0),
      completed_tasks_(0) {
    for (int i = 0; i < num_threads_; i++) {
        workers_.emplace_back([this]() {
            workerLoop();
        });
    }
}

void TaskSystemParallelThreadPoolSleeping::workerLoop() {
    while (true) {
        IRunnable *runnable;
        int task_id;
        int num_total_tasks;

        {
            std::unique_lock<std::mutex> lock(mutex_);

            work_available_cv_.wait(lock, [this]() {
                return shutdown_ ||
                       (work_available_ &&
                        next_task_ < current_num_tasks_);
            });

            if (shutdown_) {
                return;
            }

            task_id = next_task_;
            next_task_++;

            runnable = current_runnable_;
            num_total_tasks = current_num_tasks_;
        }

        runnable->runTask(task_id, num_total_tasks);

        {
            std::lock_guard<std::mutex> lock(mutex_);

            completed_tasks_++;

            if (completed_tasks_ == current_num_tasks_) {
                work_available_ = false;
                all_tasks_done_cv_.notify_one();
            }
        }
    }
}

void TaskSystemParallelThreadPoolSleeping::run(
    IRunnable *runnable,
    int num_total_tasks
) {
    if (num_total_tasks <= 0) {
        return;
    }

    std::unique_lock<std::mutex> lock(mutex_);

    current_runnable_ = runnable;
    current_num_tasks_ = num_total_tasks;
    next_task_ = 0;
    completed_tasks_ = 0;
    work_available_ = true;

    work_available_cv_.notify_all();

    all_tasks_done_cv_.wait(lock, [this]() {
        return completed_tasks_ == current_num_tasks_;
    });

    current_runnable_ = nullptr;
}

TaskSystemParallelThreadPoolSleeping::
~TaskSystemParallelThreadPoolSleeping() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
    }

    work_available_cv_.notify_all();

    for (std::thread &worker: workers_) {
        worker.join();
    }
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                                                              const std::vector<TaskID> &deps) {
    //
    // TODO: CS149 students will implement this method in Part B.
    //

    return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync() {
    //
    // TODO: CS149 students will modify the implementation of this method in Part B.
    //

    return;
}
