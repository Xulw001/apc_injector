/**
 * @file apc_util.cpp
 * @author xulw (nevermore.xulw@hotmail.com)
 * @brief util for apc operation
 * @version 1.0
 * @date 2025-09-26
 *
 * @copyright Copyright (c) 2025
 *
 */
#include "utils/apc_util.h"

#include "krtl/new.h"

namespace util {
namespace apc {

typedef struct _APC_CONTEXT {
    KAPC apc;                 ///
    BOOLEAN alert;            /// whether force delivery
    PEX_RUNDOWN_REF rundown;  /// mandatory protection for apc running
} APC_CONTEXT, *PAPC_CONTEXT;

/**
 * @brief kernel Apc routine to release resource when target process is destroyed
 */
static VOID DefaultApcRundownRoutine(_In_ PKAPC apc) {
    PAPC_CONTEXT apc_ctx = CONTAINING_RECORD(apc, APC_CONTEXT, apc);
    PEX_RUNDOWN_REF apc_rundown = apc_ctx->rundown;
    delete apc_ctx;
    ExReleaseRundownProtection(apc_rundown);
}

/**
 * @brief Normal kernel Apc routine
 */
static VOID DefaultApcKernelRoutine(_In_ PKAPC apc, _In_ PKNORMAL_ROUTINE* routine,
                                    _In_ PVOID* context, _In_ PVOID*, _In_ PVOID*) {
    PAPC_CONTEXT apc_ctx = CONTAINING_RECORD(apc, APC_CONTEXT, apc);
    if (apc_ctx->alert) {
        // force thread enters an alertable state
        KeTestAlertThread(UserMode);
    } else {
        // skip user-mode apc
        if (PsIsThreadTerminating(PsGetCurrentThread())) {
            *routine = nullptr;
        }

        // fix wow64 apc
        if (PsGetCurrentProcessWow64Process() != nullptr) {
            PsWrapApcWow64Thread(context, (PVOID*)routine);
        }
    }
    PEX_RUNDOWN_REF apc_rundown = apc_ctx->rundown;
    delete apc_ctx;
    ExReleaseRundownProtection(apc_rundown);
}

static bool QueueAPC(_In_ PETHREAD thread_ob,
                     _In_ KPROCESSOR_MODE apc_mode,
                     _In_opt_ PKNORMAL_ROUTINE routine,
                     _In_ PEX_RUNDOWN_REF apc_rundown,
                     _In_opt_ PVOID context,
                     _In_opt_ BOOLEAN alertable) {
    // whether target thread is alive
    if (PsIsThreadTerminating(thread_ob)) return false;

    if (alertable) {
        // if force-delivery, queue the user-mode APC first
        if (!QueueAPC(thread_ob, apc_mode, routine, apc_rundown, context, false)) {
            return false;
        }
        // clean the context for alert apc
        apc_mode = KernelMode;
        routine = nullptr;
        context = nullptr;
    }

    // Initialize the APC
    PAPC_CONTEXT apc_ctx = new (PoolTag::NonPaged) APC_CONTEXT;
    if (!apc_ctx) return false;
    apc_ctx->alert = alertable;
    apc_ctx->rundown = apc_rundown;

    KeInitializeApc(&apc_ctx->apc,
                    thread_ob,
                    OriginalApcEnvironment,
                    DefaultApcKernelRoutine,
                    DefaultApcRundownRoutine,
                    routine,
                    apc_mode,
                    context);
    // acquire mandatory protection for kernel apc
    if (ExAcquireRundownProtection(apc_rundown)) {
        if (KeInsertQueueApc(&apc_ctx->apc, nullptr, nullptr, IO_NO_INCREMENT)) {
            return true;
        }
        // release mandatory protection when insert apc failed
        ExReleaseRundownProtection(apc_rundown);
    }
    delete apc_ctx;
    return false;
}

bool QueueUserAPC(_In_ PETHREAD thread_ob,
                  _In_ PKNORMAL_ROUTINE routine,
                  _In_ PEX_RUNDOWN_REF apc_rundown,
                  _In_opt_ PVOID context,
                  _In_opt_ BOOLEAN alertable) {
    return QueueAPC(thread_ob, UserMode, routine, apc_rundown, context, alertable);
}
}  // namespace apc
}  // namespace util
