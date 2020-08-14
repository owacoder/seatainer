/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#include "process.h"
#include "utility.h"

/* TODO: processes are a work in progress */

#if LINUX_OS
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

struct ProcessListStruct {
    pid_t *processes;
    size_t count;
    size_t capacity;
};

struct ProcessStruct {
    ProcessNativeHandle info;
    int fdStdin, fdStdout, fdStderr;
    IO ioStdin, ioStdout, ioStderr;
    int error;
};

typedef struct ProcessListStruct *ProcessList;

static struct ProcessListStruct proclist; /* Contains list of all child processes not run synchronously (daemons are not included in this list) */
static Spinlock proclist_lock;
static Atomic proclist_purge;

static void proclist_remove(ProcessList lst, pid_t proc) {
    spinlock_lock(&proclist_lock);

    printf("proclist remove %d\n", proc);
    for (size_t i = 0; i < lst->count; ++i) {
        if (lst->processes[i] == proc) {
            for (++i; i < lst->count; ++i)
                lst->processes[i-1] = lst->processes[i];

            lst->count -= 1;
            printf("proclist removed\n");
            break;
        }
    }

    spinlock_unlock(&proclist_lock);
}

static int proclist_add(ProcessList lst, pid_t proc) {
    /* Purge zombie processes */
    if (atomic_cmpxchg(&proclist_purge, 1, 0) != 0) {
        siginfo_t info;

        while (1) {
            info.si_pid = 0;

            if (waitid(P_ALL, 0, &info, WNOHANG | WEXITED) == -1 || info.si_pid == 0)
                break;

            printf("Return code %d reaped\n", info.si_status);
            proclist_remove(&proclist, info.si_pid); /* TODO: not reentrant due to spinlock */
        }
    }

    spinlock_lock(&proclist_lock);

    /* Add new process */
    printf("proclist add %d\n", proc);
    if (lst->count == lst->capacity) {
        size_t amount = MAX(8, lst->count + (lst->count >> 1));
        pid_t *d = REALLOC(lst->processes, amount * sizeof(*d));
        if (d == NULL) {
            spinlock_unlock(&proclist_lock);
            return CC_ENOMEM;
        }

        lst->processes = d;
        lst->capacity = amount;
    }

    lst->processes[lst->count++] = proc;

    spinlock_unlock(&proclist_lock);

    return 0;
}

static void proclist_at_exit(void) {
    printf("Killing %d\n", (int) proclist.count);

    for (size_t i = 0; i < proclist.count; ++i) {
        kill(proclist.processes[i], SIGKILL);
        waitpid(proclist.processes[i], NULL, 0);
    }

    FREE(proclist.processes);
}

static void sigchld(int unused) { /* SIGCHLD handler only handles zombie processes */
    UNUSED(unused)
    atomic_set(&proclist_purge, 1);
}

static void register_proc_funcs(void) {
    static Atomic registered = 0;

    if (!atomic_set(&registered, 1)) {
        atexit(proclist_at_exit);
        signal(SIGCHLD, sigchld);
    }
}
#elif WINDOWS_OS
#include <winuser.h>
#if MSVC_COMPILER
# pragma comment(lib, "user32.lib")
#endif

struct ProcessListStruct {
    PROCESS_INFORMATION *processes;
    size_t count;
    size_t capacity;
};

struct ProcessStruct {
    ProcessNativeHandle info;
    HANDLE hStdin, hStdout, hStderr;
    IO ioStdin, ioStdout, ioStderr;
    int error;
};

typedef struct ProcessListStruct *ProcessList;

static struct ProcessListStruct proclist; /* Contains list of all child processes not run synchronously (daemons are not included in this list) */
static Atomic proclist_lock;

static int proclist_add(ProcessList lst, PROCESS_INFORMATION proc) {
    spinlock_lock(&proclist_lock);

    printf("proclist add %d\n", proc.dwProcessId);
    if (lst->count == lst->capacity) {
        size_t amount = MAX(8, lst->count + (lst->count >> 1));
        PROCESS_INFORMATION *d = REALLOC(lst->processes, amount * sizeof(*d));
        if (d == NULL) {
            spinlock_unlock(&proclist_lock);
            return CC_ENOMEM;
        }

        lst->processes = d;
        lst->capacity = amount;
    }

    lst->processes[lst->count++] = proc;

    spinlock_unlock(&proclist_lock);

    return 0;
}

static void proclist_remove(ProcessList lst, PROCESS_INFORMATION proc) {
    spinlock_lock(&proclist_lock);

    printf("proclist remove %d\n", proc.dwProcessId);
    for (size_t i = 0; i < lst->count; ++i) {
        if (lst->processes[i].hProcess == proc.hProcess) {
            for (++i; i < lst->count; ++i)
                lst->processes[i-1] = lst->processes[i];

            lst->count -= 1;
            printf("proclist removed\n");
            break;
        }
    }

    spinlock_unlock(&proclist_lock);
}

static void proclist_at_exit(void) {
    printf("Killing %d\n", proclist.count);

    for (size_t i = 0; i < proclist.count; ++i) {
        if (process_native_kill_normal(proclist.processes[i]))
            process_native_kill_immediate(proclist.processes[i]);
        CloseHandle(proclist.processes[i].hProcess);
        CloseHandle(proclist.processes[i].hThread);
    }

    FREE(proclist.processes);
}

#if 0
static void sigchld(void) { /* SIGCHLD handler only handles zombie processes */
    siginfo_t info;

    while (1) {
        info.si_pid = 0;

        if (waitid(P_ALL, 0, &info, WNOHANG | WEXITED) == -1 || info.si_pid == 0)
            return;

        printf("Return code %d reaped\n", info.si_status);
        proclist_remove(&proclist, info.si_pid);
    }
}
#endif

static void register_proc_funcs(void) {
    static Atomic registered = 0;

    if (!atomic_set(&registered, 1)) {
        /* signal(SIGCHLD, sigchld); */
        atexit(proclist_at_exit);
    }
}

char *process_arglist_to_string(const char * const args[]) {
    IO io = io_open_dynamic_buffer("wb");
    if (io == NULL)
        return NULL;

    while (args && *args) {
        // Add argument to command line
        const char *arg = *args;
        if (strpbrk(arg, " \t\\\"") != NULL) {
            size_t backslashes = 0;

            if (io_putc('"', io) == EOF)
                goto error;

            while (*arg) {
                if (*arg == '\\') {
                    ++backslashes;
                } else {
                    size_t multiplier = *arg == '"'? 2: 1;

                    for (size_t i = 0; i < backslashes * multiplier; ++i) {
                        if (io_putc('\\', io) == EOF)
                            goto error;
                    }

                    if (*arg == '"') {
                        if (io_putc('\\', io) == EOF || io_putc('"', io) == EOF)
                            goto error;
                    } else {
                        if (io_putc(*arg, io) == EOF)
                            goto error;
                    }

                    backslashes = 0;
                }

                ++arg;
            }

            if (backslashes) {
                for (size_t i = 0; i < backslashes * 2; ++i) {
                    if (io_putc('\\', io) == EOF)
                        goto error;
                }
            }

            if (io_putc('"', io) == EOF)
                goto error;
        } else {
            if (io_puts(arg, io) == EOF)
                goto error;
        }

        if (args[1] && io_putc(' ', io) == EOF)
            goto error;

        ++args;
    }

    if (io_putc(0, io) == EOF)
        goto error;

    char *str = io_take_underlying_buffer(io);
    io_close(io);

    return str;

error:
    io_close(io);
    return NULL;
}

struct WindowEnumStruct {
    ProcessNativeHandle handle;
    HWND hwnd;
};

static BOOL CALLBACK process_enum_windows(HWND hwnd, LPARAM lparam) {
    struct WindowEnumStruct *win = (struct WindowEnumStruct *) lparam;

    DWORD dwProcessId;
    GetWindowThreadProcessId(hwnd, &dwProcessId);

    if (dwProcessId == win->handle.dwProcessId) {
        win->hwnd = hwnd;
        return FALSE;
    }

    return TRUE;
}

static HWND process_top_level_window(ProcessNativeHandle handle) {
    struct WindowEnumStruct win;

    win.handle = handle;
    win.hwnd = NULL;

    EnumWindows(process_enum_windows, (LPARAM) &win);

    return win.hwnd;
}
#else
static void register_proc_funcs() {

}
#endif

int process_start_sync(const char *process, const char * const args[], int *exit_status) {
    const char * const temp_args[] = {process, NULL};

    register_proc_funcs();

#if LINUX_OS
    if (exit_status)
        *exit_status = 0;

    int pipefd[2];

    if (pipe2(pipefd, O_CLOEXEC) != 0)
        return errno;

    pid_t child = fork();
    if (child == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        return errno;
    } else if (child == 0) {
        close(pipefd[0]); /* Close read end */

        execvp(process, (char * const *) (args? args: temp_args));

        int error = errno;
        write(pipefd[1], &error, sizeof(int));
        close(pipefd[1]);
        exit(0);
    } else {
        int error = 0;
        close(pipefd[1]); /* Close write end */
        int amount_read = read(pipefd[0], &error, sizeof(int));
        if (amount_read < 0)
            error = errno;
        else if (amount_read == 0) /* Reached EOF on reading from pipe, this means the exec in the child succeeded. */
            error = 0;
        close(pipefd[0]);

        if (error)
            return error;

        siginfo_t info;

        if (waitid(P_PID, (id_t) child, &info, WEXITED) == -1)
            return errno;

        if (info.si_code == CLD_EXITED) {
            if (exit_status)
                *exit_status = info.si_status;

            return 0;
        } else {
            return CC_EINTR;
        }
    }
#elif WINDOWS_OS
    PWSTR proc = utf8_to_wide_alloc(process);
    char *commandline = process_arglist_to_string(args? args: temp_args);
    LPWSTR wcommandline = utf8_to_wide_alloc(commandline);
    STARTUPINFO startupinfo;
    PROCESS_INFORMATION procinfo;

    ZeroMemory(&startupinfo, sizeof(startupinfo));
    ZeroMemory(&procinfo, sizeof(procinfo));

    if (proc == NULL || commandline == NULL || wcommandline == NULL)
        goto out_of_memory;

    BOOL success = CreateProcessW(proc, wcommandline, NULL, NULL, FALSE, 0, NULL, NULL, &startupinfo, &procinfo);

    if (success) {
        DWORD exit_code;

        if (proclist_add(&proclist, procinfo))
            goto out_of_memory;

        if (WaitForSingleObject(procinfo.hProcess, INFINITE) == WAIT_FAILED ||
                !GetExitCodeProcess(procinfo.hProcess, &exit_code))
            goto error;

        proclist_remove(&proclist, procinfo);

        if (exit_status)
            *exit_status = exit_code;

        CloseHandle(procinfo.hProcess);
        CloseHandle(procinfo.hThread);
    }

    FREE(proc);
    FREE(commandline);
    FREE(wcommandline);

    return success? 0: GetLastError();

out_of_memory:
    FREE(proc);
    FREE(commandline);
    FREE(wcommandline);
    return CC_ENOMEM;

error: {
        int error_code = GetLastError();
        FREE(proc);
        FREE(commandline);
        FREE(wcommandline);
        proclist_remove(&proclist, procinfo);
        process_native_kill_immediate(procinfo);
        CloseHandle(procinfo.hProcess);
        CloseHandle(procinfo.hThread);
        return error_code;
    }
#else
    UNUSED(process)
    UNUSED(args)

    return CC_ENOTSUP;
#endif
}

int process_start_async(const char *process, const char * const args[], ProcessNativeHandle *handle) {
    const char * const temp_args[] = {process, NULL};

    register_proc_funcs();

#if LINUX_OS
    if (handle)
        *handle = 0;

    int pipefd[2];

    if (pipe2(pipefd, O_CLOEXEC) != 0)
        return errno;

    pid_t child = fork();
    if (child == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        return errno;
    } else if (child == 0) {
        close(pipefd[0]); /* Close read end */

        execvp(process, (char * const *) (args? args: temp_args));

        int error = errno;
        write(pipefd[1], &error, sizeof(int));
        close(pipefd[1]);
        exit(0);
    } else {
        int error = 0;
        close(pipefd[1]); /* Close write end */
        int amount_read = read(pipefd[0], &error, sizeof(int));
        if (amount_read < 0)
            error = errno;
        else if (amount_read == 0) /* Reached EOF on reading from pipe, this means the exec in the child succeeded. */
            error = 0;
        close(pipefd[0]);

        if (!error && proclist_add(&proclist, child)) {
            process_native_kill_immediate(child);
            error = CC_ENOMEM;
        }

        if (!error && handle)
            *handle = child;

        return error;
    }
#elif WINDOWS_OS
    PWSTR proc = utf8_to_wide_alloc(process);
    char *commandline = process_arglist_to_string(args? args: temp_args);
    LPWSTR wcommandline = utf8_to_wide_alloc(commandline);
    STARTUPINFO startupinfo;
    PROCESS_INFORMATION procinfo;

    ZeroMemory(&startupinfo, sizeof(startupinfo));
    ZeroMemory(&procinfo, sizeof(procinfo));

    if (proc == NULL || commandline == NULL || wcommandline == NULL)
        goto out_of_memory;

    BOOL success = CreateProcessW(proc, wcommandline, NULL, NULL, FALSE, 0, NULL, NULL, &startupinfo, &procinfo);

    if (success) {
        if (proclist_add(&proclist, procinfo))
            goto out_of_memory;

        if (handle)
            *handle = procinfo;
    }

    FREE(proc);
    FREE(commandline);
    FREE(wcommandline);

    if (success)
        return 0;

    {
        int error_code = GetLastError();
        proclist_remove(&proclist, procinfo);
        process_native_kill_immediate(procinfo);
        CloseHandle(procinfo.hProcess);
        CloseHandle(procinfo.hThread);
        return error_code;
    }

out_of_memory:
    FREE(proc);
    FREE(commandline);
    FREE(wcommandline);
    return CC_ENOMEM;
#else
    UNUSED(process)
    UNUSED(args)

    return CC_ENOTSUP;
#endif
}

int process_start_daemon(const char *process, const char * const args[], ProcessNativeHandle *handle) {
    const char * const temp_args[] = {process, NULL};

    register_proc_funcs();

#if LINUX_OS
    if (handle)
        *handle = 0;

    int pipefd[2];

    if (pipe2(pipefd, O_CLOEXEC) != 0)
        return errno;

    pid_t child = fork();
    if (child == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        return errno;
    } else if (child == 0) {
        child = fork();

        if (child == 0) {
            close(pipefd[0]); /* Close read end */
            pid_t pid = getpid();
            write(pipefd[1], &pid, sizeof(pid_t));

            execvp(process, (char * const *) (args? args: temp_args));

            int error = errno;
            write(pipefd[1], &error, sizeof(int));
            close(pipefd[1]);
            exit(0);
        } else {
            close(pipefd[0]);
            close(pipefd[1]);
            exit(0);
        }
    } else {
        pid_t pid = 0;
        int error = 0;

        close(pipefd[1]); /* Close write end */
        int amount_read = read(pipefd[0], &pid, sizeof(pid_t));
        if (amount_read < 0)
            error = errno;
        else if (amount_read != sizeof(pid_t))
            error = CC_EREAD;

        /* Read error code, if any */
        if (!error) {
            amount_read = read(pipefd[0], &error, sizeof(int));
            if (amount_read < 0)
                error = errno;
            else if (amount_read == 0) /* Reached EOF on reading from pipe, this means the exec in the child succeeded. */
                error = 0;
        }

        close(pipefd[0]);

        if (!error && handle)
            *handle = pid;

        return error;
    }
#else
    UNUSED(process)
    UNUSED(args)
    UNUSED(handle)

    return CC_ENOTSUP;
#endif
}

Process process_start(const char *process, const char *args[]) {
    const char * const temp_args[] = {process, NULL};

    Process p = CALLOC(1, sizeof(*p));
    if (p == NULL)
        return NULL;

    register_proc_funcs();

#if LINUX_OS
    p->fdStdin = p->fdStdout = p->fdStderr = -1;

    int pipefd[2] = {-1, -1};
    int stdinfd[2] = {-1, -1};
    int stdoutfd[2] = {-1, -1};
    int stderrfd[2] = {-1, -1};

    /* TODO: what if stdin, stdout, and stderr were already closed? */

    if (pipe2(pipefd, O_CLOEXEC) != 0 ||
        pipe2(stdinfd, O_CLOEXEC) != 0 ||
        pipe2(stdoutfd, O_CLOEXEC) != 0 ||
        pipe2(stderrfd, O_CLOEXEC))
        goto error_handler;

    pid_t child = fork();
    if (child == -1) {
        goto error_handler;
    } else if (child == 0) {
        close(pipefd[0]); /* Close read end of process pipe */
        close(stdinfd[1]); /* Close write end of stdin */
        close(stdoutfd[0]); /* Close read end of stdout */
        close(stderrfd[0]); /* Close read end of stderr */

        if (dup2(stdinfd[0], STDIN_FILENO) < 0 ||
            dup2(stdoutfd[1], STDOUT_FILENO) < 0 ||
            dup2(stderrfd[1], STDERR_FILENO) < 0) {
            /* An error occurred */
        } else {
            execvp(process, (char * const *) (args? args: temp_args));
        }

        int error = errno;
        write(pipefd[1], &error, sizeof(int));
        close(pipefd[1]);
        close(stdinfd[0]);
        close(stdoutfd[1]);
        close(stderrfd[1]);
        exit(0);
    } else {
        close(stdinfd[0]); stdinfd[0] = -1; /* Close read end of stdin */
        close(stdoutfd[1]); stdoutfd[1] = -1; /* Close write end of stdout */
        close(stderrfd[1]); stderrfd[1] = -1; /* Close write end of stderr */

        int error = 0;
        close(pipefd[1]); /* Close write end */
        int amount_read = read(pipefd[0], &error, sizeof(int));
        if (amount_read < 0)
            error = errno;
        else if (amount_read == 0) /* Reached EOF on reading from pipe, this means the exec in the child succeeded. */
            error = 0;
        close(pipefd[0]);

        pipefd[0] = pipefd[1] = -1; /* Reset in case we go in the error handler. */

        if (!error && proclist_add(&proclist, child)) {
            process_native_kill_immediate(child);
            error = CC_ENOMEM;
        }

        if (error) {
            errno = error;
            goto error_handler;
        } else {
            p->info = child;
            p->fdStdin = stdinfd[1];
            p->fdStdout = stdoutfd[0];
            p->fdStderr = stderrfd[0];
        }

        return p;
    }

error_handler: {
        p->error = errno;
        if (pipefd[0] >= 0) close(pipefd[0]);
        if (pipefd[1] >= 0) close(pipefd[1]);
        if (stdinfd[0] >= 0) close(stdinfd[0]);
        if (stdinfd[1] >= 0) close(stdinfd[1]);
        if (stdoutfd[0] >= 0) close(stdoutfd[0]);
        if (stdoutfd[1] >= 0) close(stdoutfd[1]);
        if (stderrfd[0] >= 0) close(stderrfd[0]);
        if (stderrfd[1] >= 0) close(stderrfd[1]);
        process_native_kill_immediate(p->info);
        return p;
    }
#elif WINDOWS_OS
    PWSTR proc = utf8_to_wide_alloc(process);
    char *commandline = process_arglist_to_string(args? args: temp_args);
    LPWSTR wcommandline = utf8_to_wide_alloc(commandline);
    STARTUPINFO startupinfo;
    PROCESS_INFORMATION procinfo;
    SECURITY_ATTRIBUTES security;

    ZeroMemory(&startupinfo, sizeof(startupinfo));
    ZeroMemory(&procinfo, sizeof(procinfo));
    ZeroMemory(&security, sizeof(security));

    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;
    security.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&startupinfo.hStdInput, &p->hStdin, &security, 0) ||
            !SetHandleInformation(p->hStdin, HANDLE_FLAG_INHERIT, 0) ||
            !CreatePipe(&p->hStdout, &startupinfo.hStdOutput, &security, 0) ||
            !SetHandleInformation(p->hStdout, HANDLE_FLAG_INHERIT, 0) ||
            !CreatePipe(&p->hStderr, &startupinfo.hStdError, &security, 0) ||
            !SetHandleInformation(p->hStderr, HANDLE_FLAG_INHERIT, 0))
        goto error;

    startupinfo.cb = sizeof(startupinfo);
    startupinfo.dwFlags |= STARTF_USESTDHANDLES;

    if (proc == NULL || commandline == NULL || wcommandline == NULL)
        goto out_of_memory;

    BOOL success = CreateProcessW(proc, wcommandline, NULL, NULL, TRUE, 0, NULL, NULL, &startupinfo, &procinfo);

    if (success) {
        if (proclist_add(&proclist, procinfo))
            goto out_of_memory;
    }

    FREE(proc);
    FREE(commandline);
    FREE(wcommandline);

    CloseHandle(startupinfo.hStdInput);
    CloseHandle(startupinfo.hStdOutput);
    CloseHandle(startupinfo.hStdError);

    p->info = procinfo;
    p->error = success? 0: GetLastError();

    return p;

out_of_memory:
    if (startupinfo.hStdInput) CloseHandle(startupinfo.hStdInput);
    if (startupinfo.hStdOutput) CloseHandle(startupinfo.hStdOutput);
    if (startupinfo.hStdError) CloseHandle(startupinfo.hStdError);
    FREE(proc);
    FREE(commandline);
    FREE(wcommandline);
    p->error = CC_ENOMEM;

    return p;

error: {
        p->error = GetLastError();
        if (startupinfo.hStdInput) CloseHandle(startupinfo.hStdInput);
        if (startupinfo.hStdOutput) CloseHandle(startupinfo.hStdOutput);
        if (startupinfo.hStdError) CloseHandle(startupinfo.hStdError);
        FREE(proc);
        FREE(commandline);
        FREE(wcommandline);
        proclist_remove(&proclist, procinfo);
        process_native_kill_immediate(procinfo);
        CloseHandle(procinfo.hProcess);
        CloseHandle(procinfo.hThread);
        return p;
    }
#else
    UNUSED(process)
    UNUSED(args)

    p->error = CC_ENOTSUP;
#endif

    return p;
}

int process_native_exists(ProcessNativeHandle handle) {
#if LINUX_OS
    if (kill(handle, 0) == 0)
        return 1;

    return 0;
#elif WINDOWS_OS
    DWORD exitCode;
    if (GetExitCodeProcess(handle.hProcess, &exitCode))
        return exitCode == STILL_ACTIVE;
#endif

    return 0;
}

int process_native_kill_normal(ProcessNativeHandle handle) {
    if (!process_native_exists(handle))
        return 0;

#if LINUX_OS
    if (handle > 0)
        return kill(handle, SIGTERM);

    return CC_EINVAL;
#elif WINDOWS_OS
    HWND process_window = process_top_level_window(handle);

    if (process_window != NULL) {
        printf("Sending message to process window\n");

        if (SendMessage(process_window, WM_CLOSE, 0, 0) != 0)
            return CC_EBADMSG;

        return 0;
    } else {
        if (AttachConsole(handle.dwProcessId)) {
            SetConsoleCtrlHandler(NULL, TRUE);
            GenerateConsoleCtrlEvent(CTRL_C_EVENT, handle.dwProcessId);

            FreeConsole();

            SetConsoleCtrlHandler(NULL, FALSE);

            return 0;
        }
    }

    return CC_EBUSY;
#else
    UNUSED(handle)
    return CC_ENOTSUP;
#endif
}

int process_native_kill_immediate(ProcessNativeHandle handle) {
    if (!process_native_exists(handle))
        return 0;

#if LINUX_OS
    if (handle > 0)
        return kill(handle, SIGKILL);

    return CC_EINVAL;
#elif WINDOWS_OS
    if (!TerminateProcess(handle.hProcess, 0))
        return GetLastError();

    return 0;
#else
    UNUSED(handle)
    return CC_ENOTSUP;
#endif
}

void process_native_close(ProcessNativeHandle handle) {
#if WINDOWS_OS
    CloseHandle(handle.hProcess);
    CloseHandle(handle.hThread);
#else
    UNUSED(handle)
#endif
}

int process_error(Process p) {
    return p->error;
}

int process_destroy(Process p) {
#if WINDOWS_OS
    int error = 0;

    CloseHandle(p->hStdin);
    CloseHandle(p->hStdout);
    CloseHandle(p->hStderr);
    io_close(p->ioStdin);
    io_close(p->ioStdout);
    io_close(p->ioStderr);

    if (process_native_kill_normal(p->info))
        error = process_native_kill_immediate(p->info);

    proclist_remove(&proclist, p->info);
    process_native_close(p->info);

    FREE(p);

    return error;

#else
    FREE(p);
    return 0;
#endif
}

IO process_stdin(Process p) {
#if LINUX_OS
    if (!p->ioStdin && p->fdStdin >= 0)
        return p->ioStdin = io_open_native_file(p->fdStdin, "wb");

    return p->ioStdin;
#elif WINDOWS_OS
    if (!p->ioStdin && p->hStdin)
        return p->ioStdin = io_open_native_file(p->hStdin, "wb");

    return p->ioStdin;
#else
    return NULL;
#endif
}

void process_close_stdin(Process p) {
#if LINUX_OS
    if (p->fdStdin >= 0) {
        io_close(p->ioStdin);
        p->ioStdin = NULL;
        close(p->fdStdin);
        p->fdStdin = -1;
    }
#elif WINDOWS_OS
    if (p->hStdin) {
        io_close(p->ioStdin);
        p->ioStdin = NULL;
        CloseHandle(p->hStdin);
        p->hStdin = NULL;
    }
#else
    UNUSED(p)
#endif
}

IO process_stdout(Process p) {
#if LINUX_OS
    if (!p->ioStdout && p->fdStdout >= 0)
        return p->ioStdout = io_open_native_file(p->fdStdout, "rb");

    return p->ioStdout;
#elif WINDOWS_OS
    if (!p->ioStdout && p->hStdout)
        return p->ioStdout = io_open_native_file(p->hStdout, "rb");

    return p->ioStdout;
#else
    return NULL;
#endif
}

IO process_stderr(Process p) {
#if LINUX_OS
    if (!p->ioStderr && p->fdStderr >= 0)
        return p->ioStderr = io_open_native_file(p->fdStderr, "rb");

    return p->ioStderr;
#elif WINDOWS_OS
    if (!p->ioStderr && p->hStderr)
        return p->ioStderr = io_open_native_file(p->hStderr, "rb");

    return p->ioStderr;
#else
    return NULL;
#endif
}

#if LINUX_OS
extern char **environ;
#endif

const char *environment_get_variable(const char *name) {
#if WINDOWS_OS
    THREAD_STATIC char buffer[65536];
    THREAD_STATIC wchar_t wbuffer[32768];

    LPWSTR wname = utf8_to_wide_alloc(name);
    if (wname == NULL)
        return NULL;

    DWORD size = GetEnvironmentVariableW(wname, wbuffer, sizeof(wbuffer)/sizeof(*wbuffer));

    FREE(wname);

    if (size == 0)
        return NULL;

    WideCharToMultiByte(CP_UTF8, 0, wbuffer, -1, buffer, sizeof(buffer), NULL, NULL);
    return buffer;

#else
    return getenv(name);
#endif
}

int environment_set_variable(const char *name, const char *value) {
#if LINUX_OS
    errno = 0;

    if (value == NULL)
        return unsetenv(name)? errno: 0;
    else
        return setenv(name, value, 1)? errno: 0;
#elif WINDOWS_OS
    LPWSTR wname = utf8_to_wide_alloc(name);
    LPWSTR wvalue = value? utf8_to_wide_alloc(value): NULL;
    int err = 0;

    if (wname == NULL || (wvalue == NULL && value != NULL))
        err = CC_ENOMEM;
    else if (!SetEnvironmentVariableW(wname, wvalue))
        err = GetLastError();

    FREE(wname);
    FREE(wvalue);

    return err;
#else
    UNUSED(name)
    UNUSED(value)

    return CC_ENOTSUP;
#endif
}

int environment_remove_variable(const char *name) {
    return environment_set_variable(name, NULL);
}

StringList environment_get_variable_list() {
#if LINUX_OS
    return stringlist_from_array((const char **) environ);
#elif WINDOWS_OS
    StringList list = stringlist_create();
    LPWCH env = GetEnvironmentStringsW();
    if (!list || !env)
        goto cleanup;

    LPWSTR var = (LPWSTR) env;

    while (*var) {
        char *utf8 = wide_to_utf8_alloc(var);

        if (utf8 == NULL || stringlist_append_move(list, utf8)) {
            FREE(utf8);
            goto cleanup;
        }

        var += lstrlenW(var) + 1;
    }

    FreeEnvironmentStringsW(env);

    return list;

cleanup:
    stringlist_destroy(list);
    if (env)
        FreeEnvironmentStringsW(env);
    return NULL;
#else
    return NULL;
#endif
}

StringMap environment_get_variable_map() {
#if LINUX_OS
    StringMap map = stringmap_create();
    if (!map)
        goto cleanup;

    for (char **env = environ; *env; ++env) {
        char *key = *env;
        char *value = strchr(*env, '=');
        if (!value)
            goto cleanup;
        *value = 0; /* TODO: not sure if environ is allowed to be modified (and modified back)? */

        if (stringmap_insert(map, key, value+1)) {
            *value = '=';
            goto cleanup;
        }

        *value = '=';
    }

    return map;

cleanup:
    stringmap_destroy(map);
    return NULL;
#elif WINDOWS_OS
    StringMap map = stringmap_create();
    LPWCH env = GetEnvironmentStringsW();
    if (!map || !env)
        goto cleanup;

    LPWSTR var = (LPWSTR) env;

    while (*var) {
        char *utf8 = wide_to_utf8_alloc(var);
        if (utf8 == NULL)
            goto cleanup;

        char *name = utf8;
        char *value = strchr(utf8, '=');
        if (!value) {
            FREE(utf8);
            goto cleanup;
        }
        *value++ = 0;

        if (stringmap_insert(map, name, value)) {
            FREE(utf8);
            goto cleanup;
        }

        FREE(utf8);
        var += lstrlenW(var) + 1;
    }

    FreeEnvironmentStringsW(env);

    return map;

cleanup:
    stringmap_destroy(map);
    if (env)
        FreeEnvironmentStringsW(env);
    return NULL;
#else
    return NULL;
#endif
}
