#ifndef INJECT_H
#define INJECT_H

#include <ntifs.h>

#include "inject_define.h"
#include "thread.h"

namespace inj {

#ifdef _WIN64
template <typename T>
inline void Set(T* a32, T* a64, T addr, bool wow64) {
    if (wow64)
        *a32 = addr;
    else
        *a64 = addr;
}

template <typename T>
inline T Get(T a32, T a64, bool wow64) {
    if (wow64)
        return a32;
    else
        return a64;
}
#else
template <typename T>
inline void Set(T* a32, T*, T addr, bool) {
    *a32 = addr;
}

template <typename T>
inline T Get(T a32, T, bool) {
    return a32;
}
#endif

// peplace target dll path instead of the value
const UNICODE_STRING inj_dll32 = RTL_CONSTANT_STRING(DLL32);
const UNICODE_STRING inj_dll64 = RTL_CONSTANT_STRING(DLL64);

#define MAX_DLL_PATH 260
class Inject {
   public:
    struct ItemInfo {
        PETHREAD thread_ob = nullptr;
        PVOID ctx = nullptr;
        bool result = true;
    };

   public:
    Inject() { ExInitializeRundownProtection(&apc_rundown); };
    ~Inject() { ExWaitForRundownProtectionRelease(&apc_rundown); }

    bool DoDllInject(_In_ PUNICODE_STRING image_name, _In_ PIMAGE_INFO image_info);
    bool DoDllInject(_In_ HANDLE pid);

   private:
    bool FindInjectThread(PEPROCESS proc_ob, PETHREAD* thread_ob);
    PVOID PrepareInject(_In_ PEPROCESS proc_ob);
    bool GetLdrLoad(_In_ PUNICODE_STRING image_name, _In_ PIMAGE_INFO image_info);
    bool GetLdrLoad(_In_ PEPROCESS proc_ob);
    bool WaitExit(_In_ PETHREAD thread_ob, _In_ PVOID ctx, _In_ BOOLEAN sync);

   private:
    void* ldrload_addr32 = nullptr;
    void* ldrload_addr64 = nullptr;
    EX_RUNDOWN_REF apc_rundown;

    thread::WorkItem<ItemInfo> worker;
};

}  // namespace inj

#endif
