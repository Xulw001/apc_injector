#ifndef PROC_UTIL_H
#define PROC_UTIL_H

#include <ntdef.h>

#include "krtl/string.h"

namespace util {
namespace proc {

/**
 * @brief Retrieves the short name of the executable file for the specified process
 * @param pid process pid
 * @return A pointer to ImageFileName in EPROCESS which should not been release
 */
LPCSTR GetProcessImageName(_In_ HANDLE pid);

/**
 * @brief Retrieves the full name of the executable image for the specified process.
 * @param pid process pid
 * @return rtl::wstring which contains the path to the executable image, maybe empty when failed.
 */
rtl::wstring GetFullProcessImageName(_In_ HANDLE pid);

/**
 * @brief Retrieves the command-line string for the specified process.
 * @param pid process pid
 * @return rtl::wstring which contains the command-line string, maybe empty when failed.
 */
rtl::wstring GetProcessCommandLine(_In_ HANDLE pid);

}  // namespace proc
}  // namespace util

#endif
