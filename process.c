#include "process.h"

#include "utility.h"

/* TODO: processes are a work in progress */

#if LINUX_OS
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

struct ProcessListStruct {
    pid_t *processes;
    size_t count;
    size_t capacity;
};

typedef struct ProcessListStruct *ProcessList;

static struct ProcessListStruct proclist; /* Contains list of all child processes not run synchronously (daemons are not included in this list) */
static Atomic proclist_lock;

static int proclist_add(ProcessList lst, pid_t proc) {
    /* Spinlock until lock is 0 */
    while (atomic_cmpxchg(&proclist_lock, 1, 0) != 0);

    printf("proclist add %d\n", proc);
    if (lst->count == lst->capacity) {
        size_t amount = MAX(8, lst->count + (lst->count >> 1));
        pid_t *d = REALLOC(lst->processes, amount * sizeof(*d));
        if (d == NULL) {
            atomic_set(&proclist_lock, 0);
            return CC_ENOMEM;
        }

        lst->processes = d;
        lst->capacity = amount;
    }

    lst->processes[lst->count++] = proc;
    atomic_set(&proclist_lock, 0);
    return 0;
}

static void proclist_remove(ProcessList lst, pid_t proc) {
    /* Spinlock until lock is 0 */
    while (atomic_cmpxchg(&proclist_lock, 1, 0) != 0);

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

    atomic_set(&proclist_lock, 0);
}

static void proclist_at_exit(void) {
    printf("Killing %d\n", proclist.count);

    for (size_t i = 0; i < proclist.count; ++i) {
        kill(proclist.processes[i], SIGKILL);
        waitpid(proclist.processes[i], NULL, 0);
    }

    FREE(proclist.processes);
}

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

static void register_proc_funcs(void) {
    static Atomic registered = 0;

    if (!atomic_set(&registered, 1)) {
        signal(SIGCHLD, sigchld);
        atexit(proclist_at_exit);
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
    /* Spinlock until lock is 0 */
    while (atomic_cmpxchg(&proclist_lock, 1, 0) != 0);

    printf("proclist add %d\n", proc.dwProcessId);
    if (lst->count == lst->capacity) {
        size_t amount = MAX(8, lst->count + (lst->count >> 1));
        PROCESS_INFORMATION *d = REALLOC(lst->processes, amount * sizeof(*d));
        if (d == NULL) {
            atomic_set(&proclist_lock, 0);
            return CC_ENOMEM;
        }

        lst->processes = d;
        lst->capacity = amount;
    }

    lst->processes[lst->count++] = proc;
    atomic_set(&proclist_lock, 0);
    return 0;
}

static void proclist_remove(ProcessList lst, PROCESS_INFORMATION proc) {
    /* Spinlock until lock is 0 */
    while (atomic_cmpxchg(&proclist_lock, 1, 0) != 0);

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

    atomic_set(&proclist_lock, 0);
}

static void proclist_at_exit(void) {
    printf("Killing %d\n", proclist.count);

    for (size_t i = 0; i < proclist.count; ++i) {
        if (process_native_kill_normal(proclist.processes[i]))
            process_native_kill_terminate(proclist.processes[i]);
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

        if (waitid(P_PID, child, &info, WEXITED) == -1)
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
        process_native_kill_terminate(procinfo);
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
    }
    else if (child == 0) {
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

        if (!error && proclist_add(&proclist, child))
            error = CC_ENOMEM;

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
        process_native_kill_terminate(procinfo);
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
    }
    else if (child == 0) {
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

    return CC_ENOTSUP;
#endif
}

Process process_start(const char *process, const char *args[]) {
    const char * const temp_args[] = {process, NULL};

    Process p = CALLOC(1, sizeof(*p));
    if (p == NULL)
        return NULL;

#if LINUX_OS

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

    BOOL success = CreateProcessW(proc, wcommandline, NULL, NULL, FALSE, 0, NULL, NULL, &startupinfo, &procinfo);

    if (success) {
        if (proclist_add(&proclist, procinfo))
            goto out_of_memory;
    }

    FREE(proc);
    FREE(commandline);
    FREE(wcommandline);

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
        process_native_kill_terminate(procinfo);
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

int process_native_kill_normal(ProcessNativeHandle handle) {
#if LINUX_OS

#elif WINDOWS_OS
    HWND process_window = process_top_level_window(handle);

    if (process_window != NULL) {
        printf("Sending message to process window\n");

        if (SendMessage(process_window, WM_CLOSE, 0, 0) != 0)
            return CC_EBADMSG;

        return 0;
    } else {
        if (AttachConsole(handle.dwProcessId)) {
            SetConsoleCtrlHandler(NULL, FALSE);
            GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, handle.dwProcessId);

            return 0;
        }
    }

    return CC_EBUSY;
#else
    UNUSED(handle)
    return CC_ENOTSUP;
#endif
}

int process_native_kill_terminate(ProcessNativeHandle handle) {
#if LINUX_OS

#elif WINDOWS_OS
    if (!TerminateProcess(handle.hProcess, 0))
        return GetLastError();

    CloseHandle(handle.hProcess);
    CloseHandle(handle.hThread);

    return 0;
#else
    UNUSED(handle)
    return CC_ENOTSUP;
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
        error = process_native_kill_terminate(p->info);

    FREE(p);

    return error;

#else
    FREE(p);
    return 0;
#endif
}

IO process_stdin(Process p) {
#if WINDOWS_OS
    if (!p->ioStdin && p->hStdin)
        return p->ioStdin = io_open_native_file(p->hStdin, "wb");

    return p->ioStdin;
#elif
    return NULL;
#endif
}

int process_close_stdin(Process p) {
#if WINDOWS_OS
    if (p->hStdin) {
        io_close(p->ioStdin);
        p->ioStdin = NULL;
        CloseHandle(p->hStdin);
        p->hStdin = NULL;
    }

    return 0;
#else
    return 0;
#endif
}

IO process_stdout(Process p) {
#if WINDOWS_OS
    if (!p->ioStdout && p->hStdout)
        return p->ioStdout = io_open_native_file(p->hStdout, "rb");

    return p->ioStdout;
#elif
    return NULL;
#endif
}

IO process_stderr(Process p) {
#if WINDOWS_OS
    if (!p->ioStderr && p->hStderr)
        return p->ioStderr = io_open_native_file(p->hStderr, "rb");

    return p->ioStderr;
#elif
    return NULL;
#endif
}
