#ifndef LOG_UTIL_H
#define LOG_UTIL_H

#include <wdm.h>

#define Log(level, fmt, ...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, level, "[%s:%d], "##fmt, __FILE__, __LINE__, __VA_ARGS__);

// Log for normal conditions
#define kInfo(fmt, ...) Log(DPFLTR_INFO_LEVEL, fmt, __VA_ARGS__)
// Log for essential under normal conditions and some non-essential in abnormal
#define kTrace(fmt, ...) Log(DPFLTR_TRACE_LEVEL, fmt, __VA_ARGS__)
// Log for abnormal conditions
#define kWarning(fmt, ...) Log(DPFLTR_WARNING_LEVEL, fmt, __VA_ARGS__)
// Log for system call error
#define kError(fmt, ...) Log(DPFLTR_ERROR_LEVEL, fmt, __VA_ARGS__)

// 使用示例

#endif