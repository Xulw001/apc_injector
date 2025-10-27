#ifndef APC_UTIL_H
#define APC_UTIL_H

#include "sys_api.h"

namespace util {
namespace apc {

/**
 * @brief Add a user-mode APC to the APC queue of the target thread
 * @param thread_ob An executive thread object structure (ETHREAD)
 * @param routine The user-mode APC function in target thread
 * @param apc_rundown The run-down protection to prevent driver unload when apc is queuing
 * @param context The parameter to the APC function
 * @param alertable Whether force thread enters an alertable state
 * @return 
 */
bool QueueUserAPC(_In_ PETHREAD thread_ob,
                  _In_ PKNORMAL_ROUTINE routine,
                  _In_ PEX_RUNDOWN_REF apc_rundown,
                  _In_opt_ PVOID context,
                  _In_opt_ BOOLEAN alertable);

}  // namespace apc
}  // namespace util

#endif
