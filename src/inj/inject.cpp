#include "inject.h"

#include "utils/apc_util.h"
#include "utils/log_util.h"
#include "utils/ps_util.h"
#include "utils/resource_util.h"

namespace inj {

using namespace util;

bool Inject::DoDllInject(_In_ PUNICODE_STRING image_name, _In_ PIMAGE_INFO image_info) {
    if (!image_name || !image_info) {
        kWarning("invalid parameter!\n");
        return false;
    }

    UNICODE_STRING ntd_32_dll = RTL_CONSTANT_STRING(L"*\\SYSWOW64\\NTDLL.DLL");
    UNICODE_STRING ntd_64_dll = RTL_CONSTANT_STRING(L"*\\SYSTEM32\\NTDLL.DLL");
    PUNICODE_STRING ntd_dll = Get(&ntd_32_dll, &ntd_64_dll, nullptr != PsGetCurrentProcessWow64Process());
    if (!(FsRtlIsNameInExpression(ntd_dll, image_name, TRUE, NULL))) {
        kInfo("dll dismatch!\n");
        return false;
    }

    if (!GetLdrLoad(image_name, image_info)) {
        kTrace("find LdrLoadDll failed!\n");
        return false;
    }

    PVOID context = PrepareInject(PsGetCurrentProcess());
    if (!context) {
        kTrace("prepare context failed!\n");
        return false;
    }

    auto thread_ob = PsGetCurrentThread();
    if (apc::QueueUserAPC(thread_ob, (PKNORMAL_ROUTINE)context, &apc_rundown, nullptr, false)) {
        ObReferenceObject(thread_ob);
        return WaitExit(thread_ob, context, false);
    }

    return false;
}

bool Inject::DoDllInject(_In_ HANDLE pid) {
    bool ret = false;
    // ResourceGuard<PEPROCESS, ObfDereferenceObject> proc_ob;
    PEPROCESS proc_ob = nullptr;
    PETHREAD thread_ob = nullptr;
    do {
        if (!NT_SUCCESS(PsLookupProcessByProcessId(pid, &proc_ob))) {
            kError("find process failed!\n");
            break;
        }

        if (!GetLdrLoad(proc_ob)) {
            kTrace("find LdrLoadDll failed!\n");
            break;
        }

        if (!FindInjectThread(proc_ob, &thread_ob)) {
            kTrace("no suitable thread find!\n");
            break;
        }

        PVOID context = PrepareInject(proc_ob);
        if (!context) {
            kTrace("prepare context failed!\n");
            break;
        }

        if (apc::QueueUserAPC(thread_ob, (PKNORMAL_ROUTINE)context, &apc_rundown, nullptr, true)) {
            ObReferenceObject(thread_ob);
            ret = WaitExit(thread_ob, context, true);
        }
    } while (false);

    if (thread_ob) {
        ObDereferenceObject(thread_ob);
    }

    if (proc_ob) {
        ObDereferenceObject(proc_ob);
    }
    return ret;
}

static bool IsInjectThread(_In_ PETHREAD thread_ob, _In_ BOOLEAN wow64) {
    PUCHAR teb64 = (PUCHAR)PsGetThreadTeb(thread_ob);
    if (!teb64 || PsIsThreadTerminating(thread_ob))
        return false;

    // Skip GUI treads. APC to GUI thread causes ZwUserGetMessage to fail
    // TEB64 + 0x78  = Win32ThreadInfo
    if (*(PULONG64)(teb64 + 0x78) != 0)
        return false;

    // Skip threads with no ActivationContext
    // Skip threads with no TLS pointer. Without TLS pointer, apc will be faild
    if (wow64) {
        PUCHAR teb32 = teb64 + 0x2000;
        // TEB32 + 0x1A8 = ActivationContextStackPointer
        // TEB64 + 0x2C = ThreadLocalStoragePointer
        if (*(PULONG32)(teb32 + 0x1A8) == 0 || *(PULONG32)(teb32 + 0x2C) == 0)
            return false;

    } else {
        // TEB64 + 0x2C8 = ActivationContextStackPointer
        // TEB64 + 0x58 = ThreadLocalStoragePointer
        if (*(PULONG64)(teb64 + 0x2C8) == 0 || *(PULONG64)(teb64 + 0x58) == 0)
            return false;
    }

    return true;
}

bool Inject::FindInjectThread(PEPROCESS proc_ob, PETHREAD* thread_ob) {
    bool find = false;
    do {
        rtl::vector<HANDLE> thread_ids;
        if (!ps::EnumProcessThreads(PsGetProcessId(proc_ob), thread_ids)) {
            kWarning("enum process thread failed!\n");
            break;
        }

        bool is_wow64 = (PsGetProcessWow64Process(proc_ob) != nullptr);

        KAPC_STATE apc_state;
        KeStackAttachProcess(proc_ob, &apc_state);
        for (auto thread_id : thread_ids) {
            if (!NT_SUCCESS(PsLookupThreadByThreadId(thread_id, thread_ob))) {
                kError("find thread failed!\n");
                continue;
            }

            if (IsInjectThread(*thread_ob, is_wow64)) {
                kTrace("find target thread = %p\n", thread_id);
                find = true;
                break;
            }
            ObDereferenceObject(*thread_ob);
            *thread_ob = nullptr;
        }
        KeUnstackDetachProcess(&apc_state);
    } while (false);
    return find;
}

bool Inject::GetLdrLoad(_In_ PUNICODE_STRING image_name, _In_ PIMAGE_INFO image_info) {
    bool wow64 = nullptr != PsGetCurrentProcessWow64Process();
    if (Get(ldrload_addr32, ldrload_addr64, wow64))
        return true;

    ResourceGuard<ps::HMODULE, ps::FreeModule> module(ps::GetModuleHandle(image_name, image_info));
    if (!module) {
        kWarning("open module failed!\n");
        return false;
    }

    PVOID ldrload_addr = ps::GetProcAddress(module, "LdrLoadDll");
    if (!ldrload_addr) {
        kWarning("find function failed!\n");
        return false;
    }
    Set(&ldrload_addr32, &ldrload_addr64, ldrload_addr, wow64);
    return true;
}

bool Inject::GetLdrLoad(_In_ PEPROCESS proc_ob) {
    bool wow64 = nullptr != PsGetProcessWow64Process(proc_ob);
    if (Get(ldrload_addr32, ldrload_addr64, wow64))
        return true;

    PVOID ldrload_addr = nullptr;
    KAPC_STATE apc_state;
    KeStackAttachProcess(proc_ob, &apc_state);
    do {
        UNICODE_STRING ntd_dll = RTL_CONSTANT_STRING(L"NTDLL.dll");
        ResourceGuard<ps::HMODULE, ps::FreeModule> module(ps::GetModuleHandle(&ntd_dll));
        if (!module) {
            kWarning("get module failed!\n");
            break;
        }

        ldrload_addr = ps::GetProcAddress(module, "LdrLoadDll");
        if (!ldrload_addr) {
            kWarning("find function failed!\n");
        }
    } while (false);

    KeUnstackDetachProcess(&apc_state);
    Set(&ldrload_addr32, &ldrload_addr64, ldrload_addr, wow64);
    return ldrload_addr != nullptr;
}

#define CALL_COMPLETE 0xC0371E7E

typedef struct _INJECT_CONTEXT {
    UCHAR code[0x200];
    union {
        UNICODE_STRING path;
        UNICODE_STRING32 path32;
    };
    wchar_t buffer[MAX_DLL_PATH];
    PVOID module;
    ULONG complete;
    NTSTATUS status;
} INJECT_CONTEXT, *PINJECT_CONTEXT;

static PVOID PrepareInject64(const PVOID ldrload) {
    static const UCHAR inject_code64[] =
        {
            0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov     rax, 0x00     +2
            0x48, 0x83, 0xEC, 0x28,                                      // sub     rsp, 0x28
            0x49, 0xB9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov     r9 , &module  +16
            0x49, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov     r8 , &path    +26
            0x48, 0x31, 0xD2,                                            // xor     rdx, rdx(DllCharacteristics)
            0x48, 0x31, 0xC9,                                            // xor     rcx, rcx(DllPath)
            0xFF, 0xD0,                                                  // call    rax
            0x48, 0xBA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rdx, &complete    +44
            0xC7, 0x02, 0x7E, 0x1E, 0x37, 0xC0,                          // mov [rdx], CALL_COMPLETE
            0x48, 0xBA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rdx, &status      +60
            0x89, 0x02,                                                  // mov [rdx], eax
            0x48, 0x83, 0xC4, 0x28,                                      // add     rsp, 0x28
            0xC3                                                         // ret
        };

    PINJECT_CONTEXT inj_ctx = nullptr;
    SIZE_T size = sizeof(INJECT_CONTEXT);

    // 4) Allocate memory on the target process calling ZwAllocateVirtualMemory
    // with the bytes of the string as the size for the allocation.
    if (!NT_SUCCESS(ZwAllocateVirtualMemory(ZwCurrentProcess(), (PVOID*)&inj_ctx, 0, &size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE))) {
        kWarning("allocate virtual memory failed!\n");
        return nullptr;
    }

    PUNICODE_STRING path = &inj_ctx->path;
    path->Length = inj_dll64.Length;
    path->MaximumLength = inj_dll64.MaximumLength;
    path->Buffer = inj_ctx->buffer;
    RtlCopyMemory(path->Buffer, inj_dll64.Buffer, inj_dll64.Length);

    RtlCopyMemory(inj_ctx, inject_code64, sizeof(inject_code64));
    *(uintptr_t*)((PUCHAR)inj_ctx + 2) = (uintptr_t)ldrload;
    *(uintptr_t*)((PUCHAR)inj_ctx + 16) = (uintptr_t)&inj_ctx->module;
    *(uintptr_t*)((PUCHAR)inj_ctx + 26) = (uintptr_t)path;
    *(uintptr_t*)((PUCHAR)inj_ctx + 44) = (uintptr_t)&inj_ctx->complete;
    *(uintptr_t*)((PUCHAR)inj_ctx + 60) = (uintptr_t)&inj_ctx->status;
    return inj_ctx;
}

static PVOID PrepareInject32(const PVOID ldrload) {
    static const UCHAR inject_code32[] =
        {
            0xB8, 0x00, 0x00, 0x00, 0x00,        // mov     eax, 0x00    +1
            0x68, 0x00, 0x00, 0x00, 0x00,        // push    &module      +6
            0x68, 0x00, 0x00, 0x00, 0x00,        // push    &path32      +11
            0x6A, 0x00,                          // push    0(DllCharacteristics)
            0x6A, 0x00,                          // push    0(DllPath)
            0xFF, 0xD0,                          // call    eax
            0xBA, 0x00, 0x00, 0x00, 0x00,        // mov edx, &complete   +22
            0xC7, 0x02, 0x7E, 0x1E, 0x37, 0xC0,  // mov [edx], CALL_COMPLETE
            0xBA, 0x00, 0x00, 0x00, 0x00,        // mov edx, &status     +33
            0x89, 0x02,                          // mov [edx], eax
            0xC3                                 // ret
        };

    PINJECT_CONTEXT inj_ctx = nullptr;
    SIZE_T size = sizeof(INJECT_CONTEXT);

    // 4) Allocate memory on the target process calling ZwAllocateVirtualMemory
    // with the bytes of the string as the size for the allocation.
    if (!NT_SUCCESS(ZwAllocateVirtualMemory(ZwCurrentProcess(), (PVOID*)&inj_ctx, 0, &size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE))) {
        kWarning("allocate virtual memory failed!\n");
        return nullptr;
    }

    PUNICODE_STRING32 path = &inj_ctx->path32;
    path->Length = inj_dll32.Length;
    path->MaximumLength = inj_dll32.MaximumLength;
    path->Buffer = (ULONG)(ULONG_PTR)inj_ctx->buffer;
    RtlCopyMemory((PVOID)path->Buffer, inj_dll32.Buffer, inj_dll32.Length);

    RtlCopyMemory(inj_ctx, inject_code32, sizeof(inject_code32));
    *(PULONG)((PUCHAR)inj_ctx + 1) = (ULONG)(ULONG_PTR)ldrload;
    *(PULONG)((PUCHAR)inj_ctx + 6) = (ULONG)(ULONG_PTR)&inj_ctx->module;
    *(PULONG)((PUCHAR)inj_ctx + 11) = (ULONG)(ULONG_PTR)path;
    *(PULONG)((PUCHAR)inj_ctx + 22) = (ULONG)(ULONG_PTR)&inj_ctx->complete;
    *(PULONG)((PUCHAR)inj_ctx + 33) = (ULONG)(ULONG_PTR)&inj_ctx->status;
    return inj_ctx;
}

PVOID Inject::PrepareInject(_In_ PEPROCESS proc_ob) {
    if (!proc_ob) return nullptr;

    KAPC_STATE apc_state;
    KeStackAttachProcess(proc_ob, &apc_state);
    PVOID addr = Get(PrepareInject32, PrepareInject64, nullptr != PsGetCurrentProcessWow64Process())(
        Get(ldrload_addr32, ldrload_addr64, nullptr != PsGetCurrentProcessWow64Process()));
    KeUnstackDetachProcess(&apc_state);
    return addr;
}

_Function_class_(KSTART_ROUTINE) static VOID WaitExit(_In_ PVOID context) {
    LARGE_INTEGER interval = {0};
    interval.QuadPart = -(5LL * 10 * 1000);  // Sleep 5 ms

    Inject::ItemInfo* info = (Inject::ItemInfo*)context;

    KAPC_STATE apc_state;
    KeStackAttachProcess(PsGetThreadProcess(info->thread_ob), &apc_state);

    for (ULONG i = 0; i < 1000; i++) {
        if (PsIsThreadTerminating(info->thread_ob)) {
            kTrace("thread[%lld] is terminated!\n", PsGetThreadId(info->thread_ob));
            info->result = false;
            break;
        }

        if (((PINJECT_CONTEXT)(info->ctx))->complete == CALL_COMPLETE)
            break;

        if (!NT_SUCCESS(KeDelayExecutionThread(KernelMode, FALSE, &interval))) {
            kWarning("injection abnormal termination!\n");
            info->result = false;
            break;
        }
    }

    PINJECT_CONTEXT ctx = (PINJECT_CONTEXT)info->ctx;
    // avoid thread terminated
    if (info->result) {
        if (ctx->complete != CALL_COMPLETE) {
            kTrace("injection failed with timeout\n");
            info->result = false;
        } else if (!NT_SUCCESS(ctx->status) || !ctx->module) {
            kTrace("injection failed with status 0x%X\n", ctx->status);
            info->result = false;
        } else {
            info->result = true;
            kInfo("inject success!\n");
        }
    }

    KeUnstackDetachProcess(&apc_state);
    ObDereferenceObject(info->thread_ob);

    return;
}

bool Inject::WaitExit(_In_ PETHREAD thread_ob, _In_ PVOID ctx, _In_ BOOLEAN sync) {
    ItemInfo info = {thread_ob, ctx};
    if (sync) {
        inj::WaitExit(&info);
        return info.result;
    }

    worker.Insert(inj::WaitExit, info);
    return true;
}

}  // namespace inj