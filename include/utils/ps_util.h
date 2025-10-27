#ifndef PS_UTIL_H
#define PS_UTIL_H

#include <ntifs.h>

#include "krtl/string.h"
#include "krtl/unique_ptr.h"
#include "krtl/vector.h"

namespace util {
namespace ps {

typedef struct ModInfo* HMODULE;

typedef INT_PTR(FAR* FARPROC)();

/**
 * @brief Retrieves the module information for the specified module
 * @param module_name The name of the loaded module (either a .dll or .exe file)
 * @return HMODULE which contains module information, must be released at last.
 * @details Before calling this function, must attach to the target process, otherwise the function will faild.
 */
HMODULE GetModuleHandle(_In_ PUNICODE_STRING module_name);

/**
 * @brief Retrieves the module information from the image_info
 * @param full_image_name The full name of the image
 * @param image_info
 * @return HMODULE
 */
HMODULE GetModuleHandle(_In_ PUNICODE_STRING full_image_name, _In_ PIMAGE_INFO image_info);

/**
 * @brief Free the module information
 * @param module
 */
void FreeModule(_In_ HMODULE module);

/**
 * @brief Retrieves the address of an exported function from the specified dynamic-link library (DLL).
 * @param module A handle to the DLL module that contains the function.
 * @param func_name The function name.
 * @return FARPROC The address of the exported function.
 * @details Before calling this function, must attach to the target process, otherwise the function will faild.
 */
FARPROC GetProcAddress(_In_ HMODULE module, _In_ LPCSTR func_name);

/**
 * @brief Retrieves each module in the specified process.
 * @param modules
 * @details Before calling this function, must attach to the target process, otherwise the function will faild.
 * @return
 */
bool EnumProcessModules(_Inout_ rtl::vector<rtl::unique_ptr<ModInfo>>& modules);

/**
 * @brief Retrieves each thread handle in the specified process.
 * @param pid Target process handle
 * @param threads
 * @return
 */
bool EnumProcessThreads(_In_ HANDLE pid, _Inout_ rtl::vector<HANDLE>& threads);

}  // namespace ps
}  // namespace util

#endif