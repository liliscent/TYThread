/* 
 * 1) runnable_queue
 * 2) mpi_blocked_queue
 *
 * In every term of Run:
 *    start:
 *      -- if runnable_queue is not empty, pop one and run
 *      -- else traverse all threads to steal
 *         a runnable_queue task
 *              -- if success, run it
 *              -- else 
 *                      -- if mpi_blocked_queue is not empty
 *                          MPI_Test
 *                      -- else
 *                           mut_wait_task
 *                           cond_wait_task.wait()
 *
 *    when a task is spawned or restarted, cond_wait_task.notify_all()
 *
 *    // TaskHandle must be a shared_ptr to Task, because 
 *    // a Task might terminate before a group wait is fired
 *    using TaskHandle = std::shared_ptr<Task>;
 *
 *              // toy and toy_component must be already in a coroutine 
 *              void toy() {
 *                  TaskHandle id1 = go(foo, a, b);
 *                  TaskHandle id2 = go(bar, c, d);
 *                  TaskHandle id3 = go(baz, c, d);
 *
 *                  TaskGroup gp;
 *                  // a potential race-condition: if a raw pointer was used, then 
 *                  // no way to determine whether id1 is still valid at this point
 *                  gp.registe(id1).registe(id2);
 *
 *                  go(toy_component, id1, id3);
 *
 *                  gp.wait();
 *              }
 *
 *              void toy_component(TaskHandle id1, TaskHandle id3) {
 *                  TaskGroup gp;
 *                  gp.registe(id1).registe(id3);
 *
 *                  // do many things 
 *                  gp.wait();
 *
 *                  // do many things encore
 *              }
 *
 *
 *              // when encounter a TaskGroup::wait(),
 *              // task become a GroupWait state;
 *              // when a task terminates, it removes itself from 
 *              // relative groups
 *              
 */

#include "GlobalMediator.hh"
#include "Task.hh"
#include "debug.hh"
#include "TaskGroup.hh"

#include <mutex>
#include <array>
#include <vector>
#include <algorithm>
#include <boost/context/all.hpp>

#ifdef _UNIT_TEST_TASK_

TaskPtr bigTestTask;

#endif /* _UNIT_TEST_TASK_ */

std::atomic<int> Task::debugId_counter = {0};

Task::~Task()
{
    DEBUG_PRINT(DEBUG_Task,
            "Task %d destructor called", debugId); 
}

void
Task::continuationOut()
{
    auto ptr = co_currentTask;
    MUST_TRUE(ptr->saved_continuation, "task: %d", ptr->debugId);
    DEBUG_PRINT(DEBUG_Task, "Thread %d: Task %d continuationOut", 
            globalMediator.thread_id, ptr->debugId);
    ptr->saved_continuation = ptr->saved_continuation.resume();
}

void
Task::addToGroup_locked(TaskGroup *gp)
{
    DEBUG_PRINT(DEBUG_Task, "Task %d addToGroup %d...", debugId, gp->debugId);
    groups.push_back(gp);
}

void
Task::removeFromGroup(TaskGroup *gp)
{
    std::lock_guard<std::mutex> _(mut_);
    DEBUG_PRINT(DEBUG_Task, "Task %d removeFromGroup %d...", debugId, gp->debugId);
    auto iter = std::find(groups.begin(), groups.end(), gp);
    groups.erase(iter);
}

void
Task::terminate()
{
    //TODO: race-condition, termiate before register
    //Done
    std::lock_guard<std::mutex> _(mut_);
    DEBUG_PRINT(DEBUG_Task, "Task %d terminating...", debugId);
    state = Terminated;

    // this code is inside critical area,
    // because TaskGroup destructor might be called
    // at the same time
    for ( auto group : groups ) {
        group->informDone(shared_from_this());
    }
}

const char*
Task::getStateName(int s)
{
    static std::array<const char *, END_OF_STATE> dict = {
        "Initial",
        "Runnable",
        "MPIBlocked",
        "GroupWait",
        "Terminated",
    };
    if ( s < dict.size() ) {
        return dict[s];
    } else {
        return "<unknown>";
    }
}

void
Task::continuationIn()
{
    DEBUG_PRINT(DEBUG_Task, "Thread %d: Task %d continuationIn with state %s...",
            globalMediator.thread_id, debugId, getStateName(state));
    proccessedAfterYield = false;
    if ( state == Task::Runnable ) {
        MUST_TRUE(task_continuation, "task: %d", co_currentTask->debugId);
        task_continuation = task_continuation.resume();
    } else if ( state == Task::Initial ) {
        state = Task::Runnable;
        task_continuation = boost::context::callcc(
            [this] (continuation_t &&cont) -> continuation_t {
                saved_continuation = std::move(cont);
                try {
                    callback();
                    terminate();
                } catch ( std::exception const &e ) {
                    // TODO: add exception handling
                    terminate();
                }
                return std::move(saved_continuation);
            });
    }
}