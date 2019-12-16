#include "process.h"

#include "utility.h"

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

static void proclist_at_exit() {
    printf("Killing %d\n", proclist.count);

    for (size_t i = 0; i < proclist.count; ++i) {
        kill(proclist.processes[i], SIGKILL);
        waitpid(proclist.processes[i], NULL, 0);
    }

    FREE(proclist.processes);
}

static void sigchld() { /* SIGCHLD handler only handles zombie processes */
    siginfo_t info;

    while (1) {
        info.si_pid = 0;

        if (waitid(P_ALL, 0, &info, WNOHANG | WEXITED) == -1 || info.si_pid == 0)
            return;

        printf("Return code %d reaped\n", info.si_status);
        proclist_remove(&proclist, info.si_pid);
    }
}

static void register_proc_funcs() {
    static Atomic registered = 0;

    if (!atomic_set(&registered, 1)) {
        signal(SIGCHLD, sigchld);
        atexit(proclist_at_exit);
    }
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
