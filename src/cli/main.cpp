#include <Windows.h>
#include <stdio.h>

#include <memory>

#include "shared/drv.h"

int wmain(int argc, wchar_t* argvs[]) {
    if (argc < 3 || argvs[1][0] != L'-' || wcslen(argvs[1]) < 2) {
        printf("Usage: [-p pid][-f path]");
        return -1;
    }

    DWORD code = 0;
    DWORD size = 0;
    std::unique_ptr<char[]> buffer;
    switch (argvs[1][1]) {
    case 'f': {
        wchar_t drive[] = {L"C:"};
        drive[0] = argvs[2][0];
        if (drive[0] > 'a') drive[0] -= ('a' + 'A');
        if (drive[0] < 'A' || drive[0] > 'Z') {
            printf("invalid drive");
            return -1;
        }

        wchar_t nt_path[MAX_PATH];
        size_t len = QueryDosDeviceW(drive, nt_path, MAX_PATH);
        if (!len) {
            printf("invalid drive");
            return -1;
        }
        len -= 2;  // remove last null

        code = IOCTL_UPDATE_INJECT_PATH;
        size = (len + wcslen(argvs[2] + 2)) * 2 + 2;
        wchar_t* p = new wchar_t[size / 2];
        *(uint16_t*)p = (uint16_t)size - 2;
        memcpy(p + 1, nt_path, len * 2);
        memcpy(p + 1 + len, argvs[2] + 2, wcslen(argvs[2] + 2) * 2);
        buffer.reset((char*)p);

    } break;
    case 'p': {
        int pid = _wtoi(argvs[2]);
        if (pid < 0 || pid > 65535) {
            printf("invalid process");
            return -1;
        }

        code = IOCTL_DO_INJECT;
        size = sizeof(uint32_t);
        uint32_t* p = new uint32_t(pid);
        buffer.reset((char*)p);
    } break;
    default:
        printf("invalid parameter");
        return -1;
    }

    HANDLE dev = CreateFileW(kSymbolName,
                            GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL,
                            OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL,
                            NULL);
    if (!dev) {
        printf("access denied(%d)!", GetLastError());
        return -1;
    }

    DWORD ret;
    if (!DeviceIoControl(dev, code, buffer.get(), size, nullptr, 0, &ret, nullptr)) {
        printf("io failed(%d)!", GetLastError());
        return -1;
    }

    return 0;
}