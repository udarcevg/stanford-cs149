#include "tasksys.h"
#include "itasksys.h"
#include <memory>
#include <mutex>
#include <thread>
#include <vector>


IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int num_threads) {}
ITaskSystem::~ITaskSystem() {}

/*
 * ================================================================
 * Serial task system implementation
 * ================================================================
 */

const char* TaskSystemSerial::name() {
    return "Serial";
}

TaskSystemSerial::TaskSystemSerial(int num_threads): ITaskSystem(num_threads) {
}

TaskSystemSerial::~TaskSystemSerial() {}

void TaskSystemSerial::run(IRunnable* runnable, int num_total_tasks) {
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                          const std::vector<TaskID>& deps) {
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemSerial::sync() {
    return;
}

/*
 * ================================================================
 * Parallel Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelSpawn::name() {
    return "Parallel + Always Spawn";
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads): ITaskSystem(num_threads) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                 const std::vector<TaskID>& deps) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemParallelSpawn::sync() {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Spinning Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSpinning::name() {
    return "Parallel + Thread Pool + Spin";
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads): ITaskSystem(num_threads) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                              const std::vector<TaskID>& deps) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSleeping::name( ) {
    return "Parallel + Thread Pool + Sleep";
}

void TaskSystemParallelThreadPoolSleeping::workerLoop() {
    while(true) {
        BulkTask* launch;
        int task_id;
        int num_total_tasks;
        IRunnable* runnable;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            work_available_.wait(lock, [this] () {
                return shutdown_ || !ready_launches_.empty();
            });

            if (shutdown_) {
                return;
            }

            TaskID launch_id = ready_launches_.front();
            launch = launches_[launch_id].get();
            task_id = launch->next_task;
            launch->next_task++;

            runnable = launch->runnable;
            num_total_tasks = launch->num_total_tasks;

            if (launch->next_task == launch->num_total_tasks) {
                ready_launches_.pop();
            }
        }

        runnable->runTask(task_id, num_total_tasks);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            launch->completed_tasks++;
            if(launch->completed_tasks==launch->num_total_tasks){
                completeLaunchLocked(launch->id);
            }
        }
    }
}

void TaskSystemParallelThreadPoolSleeping::
completeLaunchLocked(TaskID launch_id) {

    std::vector<TaskID> completion_stak;
    completion_stak.push_back(launch_id);
    bool new_work_available = false;

    while (!completion_stak.empty()) {
        TaskID current_id = completion_stak.back();
        completion_stak.pop_back();

        BulkTask* current = launches_[current_id].get();
        if (current->completed) {
            continue;
        }

        current->completed = true;
        completed_launches_++;

        for (TaskID dependent_id: current->dependents) {
            BulkTask* depended = launches_[dependent_id].get();
            depended->remaining_dependencies--;
            if (depended->remaining_dependencies == 0) {
                if (depended->num_total_tasks == 0) {
                    completion_stak.push_back(dependent_id);
                } else {
                    ready_launches_.push(dependent_id);
                    new_work_available = true;
                }
            }
        }
    }

    if (new_work_available) {
        work_available_.notify_all();
    }

    if (completed_launches_ == total_launches_) {
        all_work_done_.notify_all();
    }

}

TaskSystemParallelThreadPoolSleeping::
TaskSystemParallelThreadPoolSleeping(int num_threads)
    :ITaskSystem(num_threads),
    num_threads_(num_threads),
    shutdown_(false),
    next_launch_id_(0),
    total_launches_(0),
    completed_launches_(0)
{
    for (int i = 0; i < num_threads_; i++) {
        workers_.emplace_back([this] () {
            this->workerLoop();
        });
    }
}

TaskSystemParallelThreadPoolSleeping::
~TaskSystemParallelThreadPoolSleeping() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
    }

    work_available_.notify_all();
    for (std::thread& w: workers_) {
        w.join();
    }

}

void TaskSystemParallelThreadPoolSleeping::
run(IRunnable* runnable, int num_total_tasks) {
    std::vector<TaskID> no_dependecies;
    runAsyncWithDeps(runnable, num_total_tasks, no_dependecies);
    sync();
}


TaskID TaskSystemParallelThreadPoolSleeping::
runAsyncWithDeps(
    IRunnable* runnable,
    int num_total_tasks,
    const std::vector<TaskID>& deps
) {
    std::lock_guard<std::mutex> lock(mutex_);
    TaskID id = next_launch_id_;
    next_launch_id_++;
    std::unique_ptr<BulkTask> launch(new BulkTask());

    launch->id = id;
    launch->runnable = runnable;
    launch->num_total_tasks = num_total_tasks;
    launch->next_task = 0;
    launch->completed_tasks = 0;
    launch->remaining_dependencies = 0;
    launch->completed = false;

    for (TaskID dependencis_id: deps) {
        BulkTask* dependency = launches_[dependencis_id].get();

        if (!dependency->completed){
            launch->remaining_dependencies++;
            dependency->dependents.push_back(id);
        }
    }

    launches_.push_back(std::move(launch));
    total_launches_++;

    BulkTask* new_launch = launches_[id].get();

    if (new_launch->remaining_dependencies == 0) {
        if (new_launch->num_total_tasks == 0) {
            completeLaunchLocked(id);
        } else {
            ready_launches_.push(id);
            work_available_.notify_all();
        }
    }

    return id;

}

void TaskSystemParallelThreadPoolSleeping::sync() {
    std::unique_lock<std::mutex> lock(mutex_);
    all_work_done_.wait(lock, [this](){
       return completed_launches_ == total_launches_;
    });
    return;
}
