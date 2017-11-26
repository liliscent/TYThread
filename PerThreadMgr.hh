#ifndef _PERTHREADMGR_HH_
#define _PERTHREADMGR_HH_

#include "forward_decl.hh"
#include "util.hh"
//#include "Blockings.hh"
#include "Task.hh"
#include "Skiplist.hh"

#include <vector>
#include <memory>

class PerThreadMgr : public NonCopyable {
public:
    void addRunnable(TaskPtr &ptr) {
        runnable_queue->enqueue(ptr);
    }
    bool run_runnable();

    bool runInStackPureTask(TaskPtr &poped);

    // stealing is done in GlobalMediator

    bool run_mpi_blocked();

    // wait at this condition with timeout
    void wait_task();

    void debug_run() {
        for (;;) {
            if ( run_runnable() ) {
                continue;
            }
            if ( run_mpi_blocked() ) {
                continue;
            }
            break;
            wait_task();
        }
    }

    /* for debug */
    TaskPtr &currentTask() { return currentTask__; }
//private:
//    BlockingQueue<TaskPtr>  runnable_queue;
    std::unique_ptr<Skiplist<Task>> runnable_queue = std::make_unique<Skiplist<Task>>();

    std::vector<TaskPtr>    mpi_blocked_queue;

    TaskPtr                 currentTask__ = nullptr;
    int                     debugId;
};

#ifdef _UNIT_TEST_PER_THREAD_MGR_
extern PerThreadMgr globalTaskMgr;
#endif /* _UNIT_TEST_PER_THREAD_MGR_ */

#endif /* _PERTHREADMGR_HH_ */

