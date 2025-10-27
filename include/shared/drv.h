#ifndef DRV_H
#define DRV_H

#ifdef WIN32
#include <Windows.h>
#else
#include <wdm.h>
#endif

// Internal driver device name, cannot be used userland
LPCWSTR kDeviceName = L"\\Device\\InjCore";
// Symlink used to reach the driver, can be used userland
LPCWSTR kSymbolName = L"\\??\\InjCore";

#define IOCTL_UPDATE_INJECT_PATH CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DO_INJECT CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

#endif