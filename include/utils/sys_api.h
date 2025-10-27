/**
 * @file sys_api.h
 * @author xulw (nevermore.xulw@hotmail.com)
 * @brief Kernel exports non-public APIs and related structure definitions.
 * @version 0.1
 * @date 2025-09-08
 *
 * @copyright Copyright (c) 2025
 *
 */
#ifndef SYS_API_H
#define SYS_API_H

#include <ntifs.h>

typedef struct _SYSTEM_PROCESS_INFORMATION {
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    UCHAR Reserved1[48];
    PVOID Reserved2[3];
    HANDLE UniqueProcessId;
    PVOID Reserved3;
    ULONG HandleCount;
    UCHAR Reserved4[4];
    PVOID Reserved5[11];
    SIZE_T PeakPagefileUsage;
    SIZE_T PrivatePageCount;
    LARGE_INTEGER Reserved6[6];
} SYSTEM_PROCESS_INFORMATION, *PSYSTEM_PROCESS_INFORMATION;

typedef struct _SYSTEM_THREAD_INFORMATION {
    LARGE_INTEGER Reserved1[3];
    ULONG Reserved2;
    PVOID StartAddress;
    CLIENT_ID ClientId;
    KPRIORITY Priority;
    LONG BasePriority;
    ULONG Reserved3;
    ULONG ThreadState;
    ULONG WaitReason;
} SYSTEM_THREAD_INFORMATION, *PSYSTEM_THREAD_INFORMATION;

#define RTL_USER_PROCESS_PARAMETERS_DEFINE(bit)        \
    typedef struct _RTL_USER_PROCESS_PARAMETERS##bit { \
        ULONG MaximumLength;                           \
        ULONG Length;                                  \
        ULONG Flags;                                   \
        ULONG DebugFlags;                              \
        PVOID Reserved2[10];                           \
        UNICODE_STRING##bit ImagePathName;             \
        UNICODE_STRING##bit CommandLine;               \
    } RTL_USER_PROCESS_PARAMETERS##bit, *PRTL_USER_PROCESS_PARAMETERS##bit;

RTL_USER_PROCESS_PARAMETERS_DEFINE(64);
RTL_USER_PROCESS_PARAMETERS_DEFINE(32);

#define PEB_LDR_DATA_DEFINE(bit)                         \
    typedef struct _PEB_LDR_DATA##bit {                  \
        ULONG Length;                                    \
        BOOLEAN Initialized;                             \
        ULONG##bit SsHandle;                             \
        LIST_ENTRY##bit InLoadOrderModuleList;           \
        LIST_ENTRY##bit InMemoryOrderModuleList;         \
        LIST_ENTRY##bit InInitializationOrderModuleList; \
    } PEB_LDR_DATA##bit, *PPEB_LDR_DATA##bit;

PEB_LDR_DATA_DEFINE(64);
PEB_LDR_DATA_DEFINE(32);

#define PEB_DEFINE(bit)               \
    typedef struct _PEB##bit {        \
        UCHAR Reserved1[2];           \
        UCHAR BeingDebugged;          \
        UCHAR Reserved2[1];           \
        ULONG##bit Reserved3[2];      \
        ULONG##bit Ldr;               \
        ULONG##bit ProcessParameters; \
    } PEB##bit, *PPEB##bit;

PEB_DEFINE(64);
PEB_DEFINE(32);

#define LDR_DATA_TABLE_ENTRY(bit)                   \
    typedef struct _LDR_DATA_TABLE_ENTRY##bit {     \
        LIST_ENTRY##bit InLoadOrderLinks;           \
        LIST_ENTRY##bit InMemoryOrderLinks;         \
        LIST_ENTRY##bit InInitializationOrderLinks; \
        ULONG##bit DllBase;                         \
        ULONG##bit EntryPoint;                      \
        ULONG SizeOfImage;                          \
        UNICODE_STRING##bit FullDllName;            \
        UNICODE_STRING##bit BaseDllName;            \
        ULONG Flags;                                \
    } LDR_DATA_TABLE_ENTRY##bit, *PLDR_DATA_TABLE_ENTRY##bit;

LDR_DATA_TABLE_ENTRY(64);
LDR_DATA_TABLE_ENTRY(32);

#define TYPE_NAME(name, bit) name##bit
#define TYPE_NAME_UNIFY(name, bit) typedef TYPE_NAME(name, bit) Nx##name
#ifdef _WIN64
#define Bit 64
#else
#define Bit 32
#endif
TYPE_NAME_UNIFY(PEB, Bit);
TYPE_NAME_UNIFY(PPEB, Bit);
TYPE_NAME_UNIFY(PEB_LDR_DATA, Bit);
TYPE_NAME_UNIFY(PPEB_LDR_DATA, Bit);
TYPE_NAME_UNIFY(LDR_DATA_TABLE_ENTRY, Bit);
TYPE_NAME_UNIFY(PLDR_DATA_TABLE_ENTRY, Bit);
TYPE_NAME_UNIFY(PRTL_USER_PROCESS_PARAMETERS, Bit);

#if defined(__cplusplus)
extern "C" {
#endif

NTSYSCALLAPI NTSTATUS NTAPI ZwQuerySystemInformation(
    _In_ ULONG SystemInformationClass,
    _Inout_ PVOID SystemInformation,
    _In_ ULONG SystemInformationLength,
    _Out_opt_ PULONG ReturnLength);

#define SystemProcessInformation 5

NTSYSCALLAPI NTSTATUS NTAPI ZwQueryInformationProcess(
    _In_ HANDLE ProcessHandle,
    _In_ PROCESSINFOCLASS ProcessInformationClass,
    _Out_writes_(ProcessInformationLength) PVOID ProcessInformation,
    _In_ ULONG ProcessInformationLength,
    _Out_opt_ PULONG ReturnLength);

NTKERNELAPI PUCHAR NTAPI PsGetProcessImageFileName(
    _In_ PEPROCESS process);

NTKERNELAPI NxPPEB NTAPI PsGetProcessPeb(_In_ PEPROCESS Process);

NTKERNELAPI PVOID NTAPI PsGetThreadTeb(_In_ PETHREAD Thread);

NTKERNELAPI PEPROCESS PsGetCurrentThreadProcess(VOID);

#ifdef _WIN64
NTKERNELAPI PPEB32 NTAPI PsGetProcessWow64Process(_In_ PEPROCESS Process);
NTKERNELAPI PPEB32 NTAPI PsGetCurrentProcessWow64Process();
#else
inline PPEB32 NTAPI PsGetProcessWow64Process(_In_ PEPROCESS) { return nullptr; }
inline PPEB32 NTAPI PsGetCurrentProcessWow64Process() { return nullptr; }
#endif

// The following are the type and function definitions for APC
typedef enum _KAPC_ENVIRONMENT {
    OriginalApcEnvironment,
    AttachedApcEnvironment,
    CurrentApcEnvironment,
    InsertApcEnvironment
} KAPC_ENVIRONMENT;

typedef VOID(NTAPI* PKNORMAL_ROUTINE)(
    _In_ PVOID NormalContext,
    _In_ PVOID SystemArgument1,
    _In_ PVOID SystemArgument2);

typedef VOID(NTAPI* PKKERNEL_ROUTINE)(
    _In_ PKAPC Apc,
    _Inout_ PKNORMAL_ROUTINE* NormalRoutine,
    _Inout_ PVOID* NormalContext,
    _Inout_ PVOID* SystemArgument1,
    _Inout_ PVOID* SystemArgument2);

typedef VOID(NTAPI* PKRUNDOWN_ROUTINE)(
    _In_ PKAPC Apc);

NTKERNELAPI VOID NTAPI KeInitializeApc(
    _Out_ PRKAPC Apc,
    _In_ PETHREAD Thread,
    _In_ KAPC_ENVIRONMENT Environment,
    _In_ PKKERNEL_ROUTINE KernelRoutine,
    _In_opt_ PKRUNDOWN_ROUTINE RundownRoutine,
    _In_opt_ PKNORMAL_ROUTINE NormalRoutine,
    _In_opt_ KPROCESSOR_MODE ApcMode,
    _In_opt_ PVOID NormalContext);

NTKERNELAPI BOOLEAN NTAPI KeInsertQueueApc(
    PRKAPC Apc,
    PVOID SystemArgument1,
    PVOID SystemArgument2,
    KPRIORITY Increment);

NTKERNELAPI BOOLEAN NTAPI KeTestAlertThread(
    _In_ KPROCESSOR_MODE AlertMode);

#if defined(__cplusplus)
}
#endif

#endif