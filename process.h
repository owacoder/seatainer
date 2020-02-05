/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#ifndef PROCESS_H
#define PROCESS_H

#include "seaerror.h"
#include "ccio.h"
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

#if LINUX_OS
typedef pid_t ProcessNativeHandle;
#else
typedef PROCESS_INFORMATION ProcessNativeHandle;
#endif

struct ProcessStruct;
typedef struct ProcessStruct *Process;

/** @brief Synchronously starts a process with given arguments, and detects whether it exited correctly or not
 *
 * @param process The UTF-8 process name to start
 * @param args A UTF-8 NULL-terminated list of process arguments. The first argument must be the process name itself
 *             If @p args is NULL, an argument list containing just the process name is used instead
 * @param exit_status A pointer to an int to hold the exit status of the started process. On success (return value of 0), @p exit_status will contain the exit status of the process. This parameter can be NULL
 * @return Returns an error if the process could not be started properly. Returns 0 (no error) if the program started but crashed after initialization
 */
int process_start_sync(const char *process, const char * const args[], int *exit_status);

/** @brief Asynchronously starts a process with given arguments, and provides a native handle to reference the process later
 *
 * A process started by this function will be killed when the calling process exits.
 *
 * @param process The UTF-8 process name to start
 * @param args A UTF-8 NULL-terminated list of process arguments. The first argument must be the process name itself
 *             If @p args is NULL, an argument list containing just the process name is used instead
 * @param handle A pointer to a native handle. On success (return value of 0), @p handle will contain the native handle that refers to the started process. This parameter can be NULL
 * @return Returns an error if the process could not be started properly. Returns 0 (no error) if the program started but crashed after initialization
 */
int process_start_async(const char *process, const char * const args[], ProcessNativeHandle *handle);

/** @brief Asynchronously starts a process with given arguments as a daemon, and provides a native handle to reference the process later
 *
 * A process started by this function will not be killed when the calling process exits.
 *
 * @param process The UTF-8 process name to start
 * @param args A UTF-8 NULL-terminated list of process arguments. The first argument must be the process name itself
 *             If @p args is NULL, an argument list containing just the process name is used instead
 * @param handle A pointer to a native handle. On success (return value of 0), @p handle will contain the native handle that refers to the started process. This parameter can be NULL
 * @return Returns an error if the process could not be started properly. Returns 0 (no error) if the program started but crashed after initialization
 */
int process_start_daemon(const char *process, const char * const args[], ProcessNativeHandle *handle);

int process_native_kill_normal(ProcessNativeHandle handle);
int process_native_kill_terminate(ProcessNativeHandle handle);

Process process_start(const char *process, const char *args[]);
int process_error(Process p);
int process_destroy(Process p);

void process_wait_for_finish(Process p);

char *process_arglist_to_string(const char * const args[]);

IO process_stdin(Process p); /* writable */
int process_close_stdin(Process p);
IO process_stdout(Process p); /* readable */
IO process_stderr(Process p); /* readable */

#ifdef __cplusplus
}
#endif

#endif // PROCESS_H
