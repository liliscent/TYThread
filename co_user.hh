#ifndef _CO_USER_HH_
#define _CO_USER_HH_

#include "Task.hh"
#include "TaskGroup.hh"
#include "GlobalMediator.hh"
//#include "mpi_hooks.hh"

#include <functional>
#include <memory>
#include <utility>

class TaskHandle {
    template<class Fn, class... Args>
    friend TaskHandle
    go(Fn&& callback, Args&&... args);
    template<class Fn, class... Args>
    friend TaskHandle
    go_pure(Fn&& callback, Args&&... args);
    friend class TaskBundle;
private:
    TaskPtr ptr__;
};

class TaskBundle {
    template<class Fn, class... Args>
    friend TaskHandle
    go(Fn&& callback, Args&&... args);
    template<class Fn, class... Args>
    friend TaskHandle
    go_pure(Fn&& callback, Args&&... args);
private:
    TaskGroup group__;
public:
    TaskBundle &registe(TaskHandle &handle) {
        group__.registe(handle.ptr__);
        return *this;
    }
    TaskBundle &registe(TaskHandle &&handle) {
        return registe(handle);
    }
    void wait() {
        group__.wait();
    }
};

template<class Fn, class... Args>
TaskHandle
go(Fn&& callback, Args&&... args)
{
    TaskHandle taskHandle;
    taskHandle.ptr__ = makeRefPtr<Task>(
            std::bind(std::forward<Fn>(callback), std::forward<Args>(args)...)); 
    globalMediator.addRunnable(taskHandle.ptr__);
    return std::move(taskHandle);
}

template<class Fn, class... Args>
TaskHandle
go_pure(Fn&& callback, Args&&... args)
{
    TaskHandle taskHandle;
    taskHandle.ptr__ = makeRefPtr<Task>(
            std::bind(std::forward<Fn>(callback), std::forward<Args>(args)...)); 
    taskHandle.ptr__->setPure();
    globalMediator.addRunnable(taskHandle.ptr__);
    return std::move(taskHandle);
}

class CountDownLatch : public NonCopyable {
public:
    CountDownLatch()
        : fakeTask__(makeRefPtr<Task>([] () {}, true))
    {}
    void add(int n) {
        counter__ = n;
    }
    void down() {
        if ( --counter__ == 0 ) {
            fakeTask__->state = Task::Terminated;
            fakeGroup__.informDone(fakeTask__);
        }
    }
    void wait() {
        fakeGroup__
            .registe(fakeTask__)
            .wait();
    }
private:
    std::atomic<int>    counter__ = {0};
    TaskPtr             fakeTask__;
    TaskGroup           fakeGroup__;
};

void
co_init()
{
    GlobalMediator::Init();
}

void
co_mainloop()
{
    globalMediator.run();
}

void
co_terminate()
{
    GlobalMediator::TerminateGracefully();
}


#endif /* _CO_USER_HH_ */

