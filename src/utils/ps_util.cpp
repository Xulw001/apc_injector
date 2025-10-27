/**
 * @file ps_util.cpp
 * @author xulw (nevermore.xulw@hotmail.com)
 * @brief util for obtaining module information
 * @version 0.1
 * @date 2025-09-09
 *
 * @copyright Copyright (c) 2025
 *
 */
#include "utils/ps_util.h"

#include <ntifs.h>
#include <ntimage.h>

#include "krtl/new.h"
#include "utils/sys_api.h"

namespace util {
namespace ps {

typedef rtl::basic_string<wchar_t, rtl::allocator<wchar_t, PoolTag::Paged>> wstring;

struct ModInfo {
    PVOID mod_base = nullptr;
    PVOID entry_point = nullptr;
    ULONG image_size = 0;
    wstring dll_name;
    wstring dll_path;
};

enum class kEnumAction {
    kErrorNext,
    kContinue,
    kExit
};

typedef kEnumAction (*ModuleEnumeration)(_In_ const NxPLDR_DATA_TABLE_ENTRY entry, _In_opt_ const PUNICODE_STRING module_name, _Inout_ PVOID buffer);
static bool KiEnumModuleInfoNative(_In_ ModuleEnumeration enum_func, _In_opt_ PUNICODE_STRING module_name, _Inout_ PVOID buffer);
static bool KiEnumModuleInfoWow64(_In_ ModuleEnumeration enum_func, _In_opt_ PUNICODE_STRING module_name, _Inout_ PVOID buffer);
static kEnumAction FindModule(_In_ const NxPLDR_DATA_TABLE_ENTRY entry, _In_ const PUNICODE_STRING module_name, _Inout_ HMODULE buffer);
static kEnumAction EnumModule(_In_ const NxPLDR_DATA_TABLE_ENTRY entry, _In_opt_ const PUNICODE_STRING, _Inout_ rtl::vector<rtl::unique_ptr<ModInfo>>* buffer);

static FARPROC KiGetProcAddress(_In_ PVOID DllBase, _In_ LPCSTR FuncName);

HMODULE GetModuleHandle(_In_ PUNICODE_STRING module_name) {
    HMODULE mod_info = new (PoolTag::Paged) ModInfo;
    if (PsGetCurrentProcessWow64Process() != nullptr) {
        if (KiEnumModuleInfoWow64((ModuleEnumeration)FindModule, module_name, mod_info)) {
            return mod_info;
        }
    } else {
        if (KiEnumModuleInfoNative((ModuleEnumeration)FindModule, module_name, mod_info)) {
            return mod_info;
        }
    }
    delete mod_info;
    return nullptr;
}

HMODULE GetModuleHandle(_In_ PUNICODE_STRING full_image_name, _In_ PIMAGE_INFO image_info) {
    HMODULE mod_info = new (PoolTag::Paged) ModInfo;
    if (!image_info) return nullptr;

    mod_info->mod_base = image_info->ImageBase;
    mod_info->image_size = (ULONG)image_info->ImageSize;
    mod_info->entry_point = nullptr;

    if (full_image_name) {
        __try {
            wstring dll_path((PWCHAR)full_image_name->Buffer, full_image_name->Length / sizeof(wchar_t));
            mod_info->dll_path.swap(dll_path);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            ;
        }
    }
    return mod_info;
}

void FreeModule(_In_ HMODULE module) {
    if (!module) return;
    delete module;
}

FARPROC GetProcAddress(_In_ HMODULE module, _In_ LPCSTR func_name) {
    if (!module || !module->mod_base) return nullptr;
    return KiGetProcAddress(module->mod_base, func_name);
}

bool EnumProcessModules(_Inout_ rtl::vector<rtl::unique_ptr<ModInfo>>& modules) {
    if (!KiEnumModuleInfoNative((ModuleEnumeration)EnumModule, nullptr, &modules)) {
        return false;
    }

    if (PsGetCurrentProcessWow64Process()) {
        if (!KiEnumModuleInfoWow64((ModuleEnumeration)EnumModule, nullptr, &modules)) {
            return false;
        }
    }
    return true;
}

bool EnumProcessThreads(_In_ HANDLE pid, _Inout_ rtl::vector<HANDLE>& threads) {
    bool ret = false;
    PCHAR buffer = nullptr;
    do {
        ULONG length = 0;
        ULONG size = 0;
        NTSTATUS ns = STATUS_SUCCESS;
        while ((ns = ZwQuerySystemInformation(SystemProcessInformation, buffer, length, &size)) ==
               STATUS_INFO_LENGTH_MISMATCH) {
            length = size;
            buffer = new (PoolTag::Paged) CHAR[length];
            if (!buffer) break;
        }

        if (!NT_SUCCESS(ns)) {
            break;
        }

        PSYSTEM_PROCESS_INFORMATION sys_proc_info = PSYSTEM_PROCESS_INFORMATION(buffer);
        do {
            if (sys_proc_info->UniqueProcessId == pid) {
                PSYSTEM_THREAD_INFORMATION sys_thread_info = (PSYSTEM_THREAD_INFORMATION)(sys_proc_info + 1);
                threads.resize(sys_proc_info->NumberOfThreads);
                for (ULONG i = 0; i < sys_proc_info->NumberOfThreads; i++) {
                    threads[i] = sys_thread_info[i].ClientId.UniqueThread;
                }
                ret = true;
                break;
            }
            sys_proc_info = (PSYSTEM_PROCESS_INFORMATION)((PUCHAR)sys_proc_info + sys_proc_info->NextEntryOffset);
        } while (sys_proc_info->NextEntryOffset != 0);
    } while (false);

    if (buffer) {
        delete[] buffer;
        buffer = nullptr;
    }
    return ret;
}

static bool KiEnumModuleInfoNative(_In_ ModuleEnumeration enum_func, _In_opt_ PUNICODE_STRING module_name, _Inout_ PVOID buffer) {
    NxPPEB peb = PsGetProcessPeb(PsGetCurrentProcess());
    PPEB_LDR_DATA64 ldr = (PPEB_LDR_DATA64)peb->Ldr;
    if (!peb || !ldr) {
        return false;
    }

    kEnumAction action = kEnumAction::kErrorNext;
    __try {
        ProbeForRead(peb, sizeof(NxPEB), 1);
        ProbeForRead(ldr, sizeof(NxPEB_LDR_DATA), 1);

        PLIST_ENTRY head = (PLIST_ENTRY)&ldr->InLoadOrderModuleList;
        for (PLIST_ENTRY entry = head->Flink; entry != head; entry = entry->Flink) {
            NxPLDR_DATA_TABLE_ENTRY ldr_table_entry = CONTAINING_RECORD(entry, NxLDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
            action = enum_func(ldr_table_entry, module_name, buffer);
            if (action == kEnumAction::kExit) break;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return action != kEnumAction::kErrorNext;
}

static bool KiEnumModuleInfoWow64(_In_ ModuleEnumeration enum_func, _In_opt_ PUNICODE_STRING module_name, _Inout_ PVOID buffer) {
    PPEB32 peb = PsGetCurrentProcessWow64Process();
    PPEB_LDR_DATA32 ldr = (PPEB_LDR_DATA32)peb->Ldr;
    if (!peb || !ldr) {
        return false;
    }

    kEnumAction action = kEnumAction::kErrorNext;
    __try {
        ProbeForRead(peb, sizeof(PEB32), 1);
        ProbeForRead(ldr, sizeof(PEB_LDR_DATA32), 1);

        PLIST_ENTRY32 head = (PLIST_ENTRY32)&ldr->InLoadOrderModuleList;
        for (PLIST_ENTRY32 entry = (PLIST_ENTRY32)head->Flink; entry != head; entry = (PLIST_ENTRY32)entry->Flink) {
            PLDR_DATA_TABLE_ENTRY32 ldr_table_entry32 = CONTAINING_RECORD(entry, LDR_DATA_TABLE_ENTRY32, InLoadOrderLinks);

            NxLDR_DATA_TABLE_ENTRY ldr_table_entry;
            ldr_table_entry.DllBase = ldr_table_entry32->DllBase;
            ldr_table_entry.EntryPoint = ldr_table_entry32->EntryPoint;
            ldr_table_entry.SizeOfImage = ldr_table_entry32->SizeOfImage;
            RtlInitUnicodeString((PUNICODE_STRING)&ldr_table_entry.BaseDllName, (PWCHAR)ldr_table_entry32->BaseDllName.Buffer);
            RtlInitUnicodeString((PUNICODE_STRING)&ldr_table_entry.FullDllName, (PWCHAR)ldr_table_entry32->FullDllName.Buffer);
            action = enum_func(&ldr_table_entry, module_name, buffer);
            if (action == kEnumAction::kExit) break;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return action != kEnumAction::kErrorNext;
}

static kEnumAction FindModule(_In_ const NxPLDR_DATA_TABLE_ENTRY entry, _In_ const PUNICODE_STRING module_name, _Inout_ HMODULE buffer) {
    if (entry->BaseDllName.Buffer == NULL) {
        return kEnumAction::kErrorNext;
    }

    if (RtlCompareUnicodeString((PUNICODE_STRING)&entry->BaseDllName, module_name, TRUE) == 0) {
        buffer->mod_base = (PVOID)entry->DllBase;
        buffer->entry_point = (PVOID)entry->EntryPoint;
        buffer->image_size = entry->SizeOfImage;

        wstring dll_name((PWCHAR)entry->BaseDllName.Buffer, entry->BaseDllName.Length / sizeof(wchar_t));
        wstring dll_path((PWCHAR)entry->FullDllName.Buffer, entry->FullDllName.Length / sizeof(wchar_t));
        buffer->dll_name.swap(dll_name);
        buffer->dll_path.swap(dll_path);
        return kEnumAction::kExit;
    }

    return kEnumAction::kErrorNext;
}

static kEnumAction EnumModule(_In_ const NxPLDR_DATA_TABLE_ENTRY entry, _In_opt_ const PUNICODE_STRING, _Inout_ rtl::vector<rtl::unique_ptr<ModInfo>>* buffer) {
    HMODULE mod_info = new (PoolTag::Paged) ModInfo;
    mod_info->mod_base = (PVOID)entry->DllBase;
    mod_info->entry_point = (PVOID)entry->EntryPoint;
    mod_info->image_size = entry->SizeOfImage;

    wstring dll_name((PWCHAR)entry->BaseDllName.Buffer, entry->BaseDllName.Length / sizeof(wchar_t));
    wstring dll_path((PWCHAR)entry->FullDllName.Buffer, entry->FullDllName.Length / sizeof(wchar_t));
    mod_info->dll_name.swap(dll_name);
    mod_info->dll_path.swap(dll_path);
    buffer->emplace_back(mod_info);
    return kEnumAction::kContinue;
}

static FARPROC KiGetProcAddress(_In_ PVOID dll_base, _In_ LPCSTR func_name) {
    if (!dll_base || !func_name) return nullptr;

    __try {
        // Check PE header for magic bytes
        PIMAGE_DOS_HEADER image_dos_header = (PIMAGE_DOS_HEADER)dll_base;
        ProbeForRead(image_dos_header, sizeof(IMAGE_DOS_HEADER), 1);
        if (image_dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
            return nullptr;
        }

        // Check PE header for signature
        PIMAGE_NT_HEADERS32 image_nt_headers32 = ((PIMAGE_NT_HEADERS32)(RtlOffsetToPointer(dll_base, image_dos_header->e_lfanew)));
        PIMAGE_NT_HEADERS64 image_nt_headers64 = ((PIMAGE_NT_HEADERS64)image_nt_headers32);
        if (image_nt_headers64->Signature != IMAGE_NT_SIGNATURE) {
            return nullptr;
        }

        // Get address of Export directory
        PIMAGE_EXPORT_DIRECTORY export_table = NULL;
        if (image_nt_headers64->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
            // Check Optional Headers
            if (!(image_nt_headers64->OptionalHeader.DataDirectory[0].VirtualAddress &&
                  0 < image_nt_headers64->OptionalHeader.NumberOfRvaAndSizes)) {
                return nullptr;
            }
            export_table = (((PIMAGE_EXPORT_DIRECTORY)(PUCHAR)RtlOffsetToPointer(
                dll_base, image_nt_headers64->OptionalHeader.DataDirectory[0].VirtualAddress)));
        } else if (image_nt_headers64->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
            // Check Optional Headers
            if (!(image_nt_headers32->OptionalHeader.DataDirectory[0].VirtualAddress &&
                  0 < image_nt_headers32->OptionalHeader.NumberOfRvaAndSizes)) {
                return nullptr;
            }
            export_table = (((PIMAGE_EXPORT_DIRECTORY)(PUCHAR)RtlOffsetToPointer(
                dll_base, image_nt_headers32->OptionalHeader.DataDirectory[0].VirtualAddress)));
        } else {
            return nullptr;
        }

        // Check for export directory
        if (!(export_table)) {
            return nullptr;
        }

        PULONG name_addresses = ((PULONG)RtlOffsetToPointer(dll_base, export_table->AddressOfNames));
        for (ULONG n = 0; n < export_table->NumberOfNames; ++n) {
            LPSTR cur_func_name = ((LPSTR)RtlOffsetToPointer(dll_base, name_addresses[n]));
            if (strcmp(func_name, cur_func_name) == 0) {
                PULONG function_address = ((PULONG)RtlOffsetToPointer(dll_base, export_table->AddressOfFunctions));
                PUSHORT ordinals_address = ((PUSHORT)RtlOffsetToPointer(dll_base, export_table->AddressOfNameOrdinals));
                return ((FARPROC)RtlOffsetToPointer(dll_base, function_address[ordinals_address[n]]));
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ;
    }
    return nullptr;
}

}  // namespace ps
}  // namespace util