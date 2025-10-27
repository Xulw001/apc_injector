#include <Windows.h>

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
    const char* ptr = "";
    switch (dwReason) {
    case DLL_PROCESS_ATTACH:
        ptr = "dll load";
        break;
    case DLL_PROCESS_DETACH:
        ptr = "dll unload";
        break;
    case DLL_THREAD_ATTACH:
        ptr = "thread load";
        break;
    case DLL_THREAD_DETACH:
        ptr = "thread unload";
        break;
    default:
        break;
    }
    OutputDebugStringA(ptr);
    return TRUE;
}
