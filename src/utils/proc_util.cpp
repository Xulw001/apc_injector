/**
 * @file proc_util.cpp
 * @author xulw (nevermore.xulw@hotmail.com)
 * @brief Util for obtaining process basic information
 * @version 0.1
 * @date 2025-09-08
 *
 * @copyright Copyright (c) 2025
 *
 */
#include "utils/proc_util.h"

#include <ntifs.h>

#include "utils/sys_api.h"

namespace util {
namespace proc {

#if 0
static PVOID QueryProcessInfomation(_In_ HANDLE pid,
                                    _In_ PROCESSINFOCLASS proc_info_class,
                                    _In_opt_ PVOID proc_info_buffer,
                                    _In_opt_ ULONG proc_info_length) {
    NTSTATUS ns = STATUS_SUCCESS;
    ULONG return_length = 0;
    while ((ns = ZwQueryInformationProcess(pid, proc_info_class, NULL, 0, &return_length)) ==
           STATUS_INFO_LENGTH_MISMATCH) {
        if (proc_info_buffer) break;

        proc_info_length = return_length;
        proc_info_buffer = new (PoolTag::Paged) UCHAR[return_length];
    }

    return NT_SUCCESS(ns) ? proc_info_buffer : nullptr;
}
#endif

LPCSTR GetProcessImageName(_In_ HANDLE pid) {
    LPCSTR image_name = nullptr;
    do {
        PEPROCESS ob_proc = nullptr;
        if (!NT_SUCCESS(PsLookupProcessByProcessId(pid, &ob_proc))) {
            break;
        }

        image_name = (PCSTR)PsGetProcessImageFileName(ob_proc);
        ObDereferenceObject(ob_proc);
    } while (false);
    return image_name;
}

rtl::wstring GetFullProcessImageName(_In_ HANDLE pid) {
    rtl::wstring full_image_name;
    do {
        PEPROCESS ob_proc = nullptr;
        if (!NT_SUCCESS(PsLookupProcessByProcessId(pid, &ob_proc))) {
            break;
        }

        PUNICODE_STRING proc_image_name = NULL;
        SeLocateProcessImageName(ob_proc, &proc_image_name);
        if (proc_image_name && proc_image_name->Length) {
            __try {
                rtl::wstring name(proc_image_name->Buffer, proc_image_name->Length / sizeof(wchar_t));
                full_image_name.swap(name);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                ;
            }
        };
        ExFreePoolWithTag(proc_image_name, 0);
        ObDereferenceObject(ob_proc);
    } while (false);
    return full_image_name;
}

rtl::wstring GetProcessCommandLine(_In_ HANDLE pid) {
    rtl::wstring proc_cmd_line;
    PEPROCESS ob_proc = nullptr;
    if (!NT_SUCCESS(PsLookupProcessByProcessId(pid, &ob_proc))) {
        return proc_cmd_line;
    }

    KAPC_STATE apc_state;
    KeStackAttachProcess(ob_proc, &apc_state);

    NxPPEB peb = PsGetProcessPeb(ob_proc);
    NxPRTL_USER_PROCESS_PARAMETERS peb_parameter = (NxPRTL_USER_PROCESS_PARAMETERS)peb->ProcessParameters;
    if (peb && peb->ProcessParameters) {
        PWCH cmd_line_buffer = nullptr;
        if (peb_parameter->Flags & 1) {
            cmd_line_buffer = (PWCH)peb_parameter->CommandLine.Buffer;
        } else {
            cmd_line_buffer = (PWCH)(peb->ProcessParameters + peb_parameter->CommandLine.Buffer);
        }

        __try {
            if (cmd_line_buffer && peb_parameter->CommandLine.Length) {
                rtl::wstring cmd_line(cmd_line_buffer, peb_parameter->CommandLine.Length / sizeof(wchar_t));
                proc_cmd_line.swap(cmd_line);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            ;
        }
    }

    KeUnstackDetachProcess(&apc_state);
    ObDereferenceObject(ob_proc);
    return proc_cmd_line;
}

}  // namespace proc
}  // namespace util