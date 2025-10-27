#include "inject.h"
#include "krtl/new.h"
#include "krtl/string.h"
#include "shared/drv.h"
#include "smap.h"
#include "utils/log_util.h"
#include "utils/proc_util.h"

using wstring = rtl::basic_string<wchar_t, rtl::allocator<wchar_t, PoolTag::Paged>>;

struct ProcBasicInfo {
    HANDLE pid = nullptr;
    wstring name;
    HANDLE ppid = nullptr;
    wstring parent_name;
    bool observe = false;
    bool injected = false;
};

struct _InjectContext {
    bool inject = false;
    inj::Inject cinj;
    rtl::wstring inject_path;
    SMap<HANDLE, ProcBasicInfo> proc_map;
}* inject_ctx = nullptr;

static bool DoInject(rtl::unordered_map<HANDLE, ProcBasicInfo>::iterator& it, PUNICODE_STRING image_name, PIMAGE_INFO image_info) {
    if (it->second.observe && !it->second.injected) {
        it->second.injected = inject_ctx->cinj.DoDllInject(image_name, image_info);
    }
    return it->second.injected;
}

static VOID NotifyOnLoadImage(_In_opt_ PUNICODE_STRING image_name, _In_ HANDLE pid, _In_ PIMAGE_INFO image_info) {
    if (!image_name) return;

    if (inject_ctx->proc_map.MapUpdateValue(pid, DoInject, image_name, image_info)) {
        kInfo("[%lld] do injecting!\n", (intptr_t)pid);
    }
}

static VOID NotifyOnCreateProcess(_In_ HANDLE ppid, _In_ HANDLE pid, _In_ BOOLEAN create) {
    if (create) {
        ProcBasicInfo proc_info;
        proc_info.pid = pid;
        proc_info.name = util::proc::GetFullProcessImageName(pid);
        proc_info.ppid = ppid;
        proc_info.parent_name = util::proc::GetFullProcessImageName(ppid);
        if (inject_ctx->inject_path.size()) {
            if (_wcsicmp(inject_ctx->inject_path.c_str(), proc_info.name.c_str()) == 0) {
                proc_info.observe = true;
            } else {
                ProcBasicInfo parent_proc_info;
                if (inject_ctx->proc_map.MapFindValue(ppid, parent_proc_info)) {
                    proc_info.observe = parent_proc_info.observe;
                }
            }
        }
        inject_ctx->proc_map.MapAddKey(pid, proc_info);
    } else {
        inject_ctx->proc_map.MapDelKey(pid);
    }
}

static NTSTATUS DeviceIoControl(_In_ PIRP irp) {
    PIO_STACK_LOCATION pisl = IoGetCurrentIrpStackLocation(irp);
    auto size = pisl->Parameters.DeviceIoControl.InputBufferLength;
    if (size < sizeof(UINT32) || !irp->AssociatedIrp.SystemBuffer) {
        return STATUS_INVALID_PARAMETER;
    }

    switch (pisl->Parameters.DeviceIoControl.IoControlCode) {
    case IOCTL_UPDATE_INJECT_PATH:
        __try {
            PUINT16 buffer = (PUINT16)irp->AssociatedIrp.SystemBuffer;
            wstring path((LPWSTR)buffer + 1, *buffer / sizeof(wchar_t));
            path.upper();
            inject_ctx->inject_path.swap(path);
            inject_ctx->inject = true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return STATUS_UNSUCCESSFUL;
        }
        break;
    case IOCTL_DO_INJECT:
        if (!inject_ctx->cinj.DoDllInject((HANDLE) * (PUINT32)irp->AssociatedIrp.SystemBuffer)) {
            return STATUS_UNSUCCESSFUL;
        }
        break;
    default:
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    return STATUS_SUCCESS;
}

_Function_class_(DRIVER_DISPATCH) static NTSTATUS DriverDispatcher(_In_ PDEVICE_OBJECT,
                                                                   _Inout_ PIRP irp) {
    NTSTATUS status = STATUS_SUCCESS;
    PIO_STACK_LOCATION pisl = IoGetCurrentIrpStackLocation(irp);
    if (pisl->MajorFunction == IRP_MJ_DEVICE_CONTROL) {
        status = DeviceIoControl(irp);
    }

    irp->IoStatus.Status = status;
    irp->IoStatus.Information = 0;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

_Function_class_(DRIVER_UNLOAD) static VOID DriverUnload(_In_ PDRIVER_OBJECT DriverObject) {
    PsSetCreateProcessNotifyRoutine(NotifyOnCreateProcess, TRUE);
    PsRemoveLoadImageNotifyRoutine(NotifyOnLoadImage);

    // Delete the driver device
    IoDeleteDevice(DriverObject->DeviceObject);

    // Delete the symbolic link
    UNICODE_STRING dev_sym_name;
    RtlInitUnicodeString(&dev_sym_name, kSymbolName);
    IoDeleteSymbolicLink(&dev_sym_name);

    if (inject_ctx) {
        delete inject_ctx;
        inject_ctx = nullptr;
    }
}

static NTSTATUS DriverLoad(_In_ PDRIVER_OBJECT DriverObject) {
    for (size_t i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; ++i) {
        DriverObject->MajorFunction[i] = DriverDispatcher;
    }
    DriverObject->DriverUnload = DriverUnload;

    NTSTATUS status = PsSetLoadImageNotifyRoutine(NotifyOnLoadImage);
    if (!NT_SUCCESS(status)) return status;

    status = PsSetCreateProcessNotifyRoutine(NotifyOnCreateProcess, FALSE);
    if (!NT_SUCCESS(status)) return status;

    UNICODE_STRING dev_name;
    UNICODE_STRING dev_sym_name;
    RtlInitUnicodeString(&dev_name, kDeviceName);
    RtlInitUnicodeString(&dev_sym_name, kSymbolName);

    PDEVICE_OBJECT device_ob;
    status = IoCreateDevice(DriverObject, 0, &dev_name, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE,
                            &device_ob);
    if (!NT_SUCCESS(status)) return status;

    status = IoCreateSymbolicLink(&dev_sym_name, &dev_name);
    if (!NT_SUCCESS(status)) return status;

    return status;
}

_Function_class_(DRIVER_INITIALIZE)
    _IRQL_requires_same_
    _IRQL_requires_(PASSIVE_LEVEL)
EXTERN_C NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject,
                              _In_ PUNICODE_STRING) {
    inject_ctx = new (PoolTag::NonPagedExecute) _InjectContext;
    if (!inject_ctx) return STATUS_NO_MEMORY;
    if (!NT_SUCCESS(DriverLoad(DriverObject))) {
        DriverUnload(DriverObject);
        return STATUS_UNSUCCESSFUL;
    }
    return STATUS_SUCCESS;
}