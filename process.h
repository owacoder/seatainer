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
typedef int ProcessNativeHandle;
#endif

struct ProcessStruct;
typedef struct ProcessStruct *Process;

int process_start_sync(const char *process, const char * const args[], int *exit_status);
int process_start_async(const char *process, const char * const args[], ProcessNativeHandle *handle);
int process_start_daemon(const char *process, const char * const args[], ProcessNativeHandle *handle);

Process process_start(const char *process, const char *args[]);
int process_error(Process p);
int process_stop(Process p);

void process_wait_for_finish(Process p);

IO process_stdin(Process p); /* writable */
IO process_stdout(Process p); /* readable */
IO process_stderr(Process p); /* readable */

#ifdef __cplusplus
}
#endif

#endif // PROCESS_H
