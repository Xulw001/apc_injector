#ifndef THREAD_H
#define THREAD_H

#include <ntifs.h>

#include "krtl/list.h"
#include "utils/log_util.h"

namespace thread {

template <typename T>
class WorkItem {
   public:
    WorkItem() {
        KeInitializeEvent(&event, NotificationEvent, FALSE);
        ExInitializeFastMutex(&mutex);
    }

    ~WorkItem() {
        runing = false;
        KeSetEvent(&event, 0, FALSE);
        if (ob_thread) {
            KeWaitForSingleObject(ob_thread, Executive, KernelMode, FALSE, NULL);
            ObDereferenceObject(ob_thread);
        }
    }

    void Insert(PKSTART_ROUTINE routine, const T& context) {
        Item item = {routine, context};
        ExAcquireFastMutex(&mutex);
        queue.emplace_back(item);
        if (!ob_thread) Initialize();
        ExReleaseFastMutex(&mutex);

        KeSetEvent(&event, 0, FALSE);
    }

    void Initialize() {
        HANDLE thread;
        if (!NT_SUCCESS(PsCreateSystemThread(&thread, THREAD_ALL_ACCESS, NULL, NULL, NULL, (PKSTART_ROUTINE)&Start, (PVOID)this))) {
            kError("WorkItem initialize failed!\n");
            return;
        }

        ObReferenceObjectByHandle(thread, THREAD_ALL_ACCESS, NULL, KernelMode, (PVOID*)&ob_thread, NULL);
        ZwClose(thread);
        kTrace("WorkItem initialize success!\n");
    }

    _Function_class_(KSTART_ROUTINE) static void Start(PVOID context) {
        auto worker = (WorkItem*)context;
        worker->Run();
        kTrace("WorkItem finalized!\n");
        PsTerminateSystemThread(STATUS_SUCCESS);
    }

    void Run() {
        while (runing) {
            KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
            if (!runing) break;

            ExAcquireFastMutex(&mutex);
            auto it = queue.front();
            queue.pop_front();
            ExReleaseFastMutex(&mutex);
            KeClearEvent(&event);

            it.routine(&it.context);
            kInfo("routine finalized!\n");
        }
    }

   private:
    struct Item {
        PKSTART_ROUTINE routine;
        T context;
    };

    rtl::list<Item, rtl::allocator<Item, PoolTag::Paged>> queue;
    KEVENT event;
    FAST_MUTEX mutex;
    PKTHREAD ob_thread = nullptr;
    bool runing = true;
};
;
}  // namespace thread

#endif