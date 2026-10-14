#include "executor.h"
#include "error.h"
#include "event.h"
#include "job.h"
#include "log.h"
#include "sig.h"
#include "terminal.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int setpgid_child(pid_t pgid)
{
    while (setpgid(0, pgid) < 0) {
        if (errno == EINTR) continue;
        return -1;
    }

    return 0;
}

static int setpgid_parent(pid_t pid, pid_t pgid)
{
    while (setpgid(pid, pgid) < 0) {
        if (errno == EINTR) continue;

        if (errno == EACCES || errno == ESRCH)
            return 0;

        return -1;
    }

    return 0;
}

static int set_cloexec(int fd)
{
    int flags = fcntl(fd, F_GETFD);

    if (flags < 0)
        return -1;

    return fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

static int startup_gate_create(int gate[2])
{
    if (pipe(gate) < 0)
        return -1;

    if (set_cloexec(gate[0]) < 0 || set_cloexec(gate[1]) < 0) {
        close(gate[0]);
        close(gate[1]);

        gate[0] = -1;
        gate[1] = -1;

        return -1;
    }

    return 0;
}

static void child_block_startup_signals(void)
{
    sigset_t set;

    sigemptyset(&set);

    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGTSTP);
    sigaddset(&set, SIGTTIN);
    sigaddset(&set, SIGTTOU);

    sigprocmask(SIG_BLOCK, &set, NULL);
}

static int startup_gate_wait(int fd)
{
    char value;

    while (1) {
        ssize_t ret = read(fd, &value, 1);

        if (ret == 1)
            return value == 'G' ? 0 : -1;

        if (ret == 0)
            return -1;

        if (errno == EINTR)
            continue;

        return -1;
    }
}

static int startup_gate_release(int fd, int count)
{
    for (int i = 0; i < count; i++) {
        char value = 'G';

        while (1) {
            ssize_t ret = write(fd, &value, 1);

            if (ret == 1)
                break;

            if (ret < 0 && errno == EINTR)
                continue;

            return -1;
        }
    }

    return 0;
}

static void child_prepare_before_gate(int gate_read, int gate_write, pid_t pgid,
                                      ShellContext *ctx)
{
    close(gate_write);

    child_block_startup_signals();

    if (setpgid_child(pgid) < 0)
        _exit(1);

    event_close_in_child();

    if (device_manager_close_all(&ctx->devices) < 0)
        _exit(1);

    if (startup_gate_wait(gate_read) < 0)
        _exit(1);

    close(gate_read);

    signal_reset_child();
}

static void reap_pid(pid_t pid)
{
    if (pid <= 0)
        return;

    while (1) {
        pid_t ret = waitpid(pid, NULL, 0);

        if (ret == pid)
            return;

        if (ret < 0 && errno == EINTR)
            continue;

        if (ret < 0 && errno == ECHILD)
            return;

        return;
    }
}

static void rollback_single(pid_t pid)
{
    if (pid <= 0)
        return;

    if (kill(-pid, SIGKILL) < 0 && errno != ESRCH) {
        log_error("failed kill child group %d during rollback: %s",
                  pid, strerror(errno));
    }

    if (kill(pid, SIGKILL) < 0 && errno != ESRCH) {
        log_error("failed kill child %d during rollback: %s",
                  pid, strerror(errno));
    }

    reap_pid(pid);
}

static void rollback_pipeline(pid_t pgid, pid_t *pids, int count)
{
    if (pgid > 0 && kill(-pgid, SIGKILL) < 0 && errno != ESRCH) {
        log_error("failed kill pipeline group %d during rollback: %s",
                  pgid, strerror(errno));
    }

    for (int i = 0; i < count; i++) {
        if (pids[i] <= 0)
            continue;

        if (kill(pids[i], SIGKILL) < 0 && errno != ESRCH) {
            log_error("failed kill pipeline child %d during rollback: %s",
                      pids[i], strerror(errno));
        }
    }

    for (int i = 0; i < count; i++)
        reap_pid(pids[i]);
}

static int check_job_limit(ShellContext *ctx)
{
    if (!ctx)
        return MiniShell_ERR_UNKNOWN;

    if (job_count_active(&ctx->jobs) >= ctx->config.max_job) {
        log_error("maximum active jobs reached: %d",
                  ctx->config.max_job);

        return MiniShell_ERR_JOB;
    }

    return MiniShell_OK;
}

static int redirect_fd(int fd, int target)
{
    if (fd == target)
        return MiniShell_OK;

    while (dup2(fd, target) < 0) {
        if (errno == EINTR)
            continue;

        return MiniShell_ERR_DUP2;
    }

    close(fd);

    return MiniShell_OK;
}

static int exec_status_from_errno(int error)
{
    return error == ENOENT ? 127 : 126;
}

static void run_process(Command *com)
{
    execvp(com->argv[0], com->argv);

    int saved_errno = errno;

    fprintf(stderr,
            "minishell: %s: %s\n",
            com->argv[0],
            strerror(saved_errno));

    _exit(exec_status_from_errno(saved_errno));
}

static int finish_foreground_job(ShellContext *ctx, Job *job, int terminal_active)
{
    if (!ctx || !job)
        return MiniShell_ERR_UNKNOWN;

    if (terminal_active && job->status == JOB_STOPPED) {
        if (terminal_get_modes(&job->terminal_modes) == 0)
            job->terminal_modes_valid = 1;
    }

    if (terminal_active && terminal_restore() < 0) {
        log_error("failed restore terminal: %s",
                  strerror(errno));

        ctx->running = 0;

        return MiniShell_ERR_UNKNOWN;
    }

    if (job->status == JOB_STOPPED) {
        printf("\n[%d]+ Stopped %s\n",
               job->id,
               job->command);

        return MiniShell_OK;
    }

    if (job->status != JOB_DONE)
        return MiniShell_ERR_UNKNOWN;

    int status = job_exit_status(job);
    pid_t pgid = job->pgid;

    job_remove(&ctx->jobs, pgid);

    return status < 0 ? MiniShell_ERR_UNKNOWN : status;
}

int execute_command(Command *com, ShellContext *ctx)
{
    if (!com || !ctx)
        return MiniShell_ERR_UNKNOWN;

    int ret = check_job_limit(ctx);

    if (ret != MiniShell_OK)
        return ret;

    return com->next
               ? execute_pipeline(com, ctx)
               : execute_single(com, ctx);
}

int setredirect(Command *com)
{
    if (!com)
        return MiniShell_ERR_UNKNOWN;

    if (com->redirect.output_file) {
        int flags = com->redirect.append
                        ? O_WRONLY | O_CREAT | O_APPEND
                        : O_WRONLY | O_CREAT | O_TRUNC;

        int fd = open(com->redirect.output_file,
                      flags,
                      0644);

        if (fd < 0) {
            log_error("open output '%s' failed: %s",
                      com->redirect.output_file,
                      strerror(errno));

            return MiniShell_ERR_OPEN;
        }

        if (redirect_fd(fd, STDOUT_FILENO) != MiniShell_OK) {
            int saved_errno = errno;

            if (fd != STDOUT_FILENO)
                close(fd);

            log_error("redirect stdout failed: %s",
                      strerror(saved_errno));

            return MiniShell_ERR_DUP2;
        }
    }

    if (com->redirect.input_file) {
        int fd = open(com->redirect.input_file,
                      O_RDONLY);

        if (fd < 0) {
            log_error("open input '%s' failed: %s",
                      com->redirect.input_file,
                      strerror(errno));

            return MiniShell_ERR_OPEN;
        }

        if (redirect_fd(fd, STDIN_FILENO) != MiniShell_OK) {
            int saved_errno = errno;

            if (fd != STDIN_FILENO)
                close(fd);

            log_error("redirect stdin failed: %s",
                      strerror(saved_errno));

            return MiniShell_ERR_DUP2;
        }
    }

    return MiniShell_OK;
}

int execute_single(Command *com, ShellContext *ctx)
{
    if (!com || !ctx)
        return MiniShell_ERR_UNKNOWN;

    int gate[2] = {-1, -1};

    if (startup_gate_create(gate) < 0) {
        log_error("failed create startup gate: %s",
                  strerror(errno));

        return MiniShell_ERR_PIPE;
    }

    int terminal_active = terminal_is_initialized();

    pid_t pid = fork();

    if (pid < 0) {
        int saved_errno = errno;

        close(gate[0]);
        close(gate[1]);

        log_error("fork failed: %s",
                  strerror(saved_errno));

        return MiniShell_ERR_FORK;
    }

    if (pid == 0) {
        child_prepare_before_gate(gate[0],
                                  gate[1],
                                  0,
                                  ctx);

        if (setredirect(com) != MiniShell_OK)
            _exit(1);

        run_process(com);
    }

    close(gate[0]);

    if (setpgid_parent(pid, pid) < 0) {
        int saved_errno = errno;

        close(gate[1]);

        log_error("failed setpgid for child %d: %s",
                  pid,
                  strerror(saved_errno));

        rollback_single(pid);

        return MiniShell_ERR_UNKNOWN;
    }

    Job *job = job_add(&ctx->jobs,
                       pid,
                       com->argv[0]);

    if (!job) {
        close(gate[1]);
        rollback_single(pid);

        return MiniShell_ERR_JOB;
    }

    if (process_add(job, pid) < 0) {
        close(gate[1]);

        rollback_single(pid);

        job_remove(&ctx->jobs, pid);

        return MiniShell_ERR_JOB;
    }

    if (!com->background &&
        terminal_active &&
        terminal_set_foreground(pid) < 0) {
        int saved_errno = errno;

        close(gate[1]);

        log_error("failed give terminal to pgid %d: %s",
                  pid,
                  strerror(saved_errno));

        rollback_single(pid);

        job_remove(&ctx->jobs, pid);

        terminal_restore();

        return MiniShell_ERR_UNKNOWN;
    }

    if (startup_gate_release(gate[1], 1) < 0) {
        close(gate[1]);

        rollback_single(pid);

        job_remove(&ctx->jobs, pid);

        if (!com->background && terminal_active)
            terminal_restore();

        return MiniShell_ERR_UNKNOWN;
    }

    close(gate[1]);

    if (com->background)
        return MiniShell_OK;

    if (job_wait_foreground(job) < 0) {
        rollback_single(pid);

        if (terminal_active)
            terminal_restore();

        job_remove(&ctx->jobs, pid);

        return MiniShell_ERR_UNKNOWN;
    }

    return finish_foreground_job(ctx,
                                 job,
                                 terminal_active);
}

int execute_pipeline(Command *com, ShellContext *ctx)
{
    if (!com || !ctx)
        return MiniShell_ERR_UNKNOWN;

    int gate[2] = {-1, -1};

    if (startup_gate_create(gate) < 0) {
        log_error("failed create pipeline startup gate: %s",
                  strerror(errno));

        return MiniShell_ERR_PIPE;
    }

    Command *current = com;

    int pipe_fd = -1;
    int count = 0;
    int terminal_active = terminal_is_initialized();

    pid_t pids[MAX_PIPELINE];
    pid_t pgid = 0;

    for (int i = 0; i < MAX_PIPELINE; i++)
        pids[i] = -1;

    while (current) {
        int pipefd[2] = {-1, -1};

        if (count >= MAX_PIPELINE) {
            if (pipe_fd >= 0)
                close(pipe_fd);

            close(gate[0]);
            close(gate[1]);

            rollback_pipeline(pgid,
                              pids,
                              count);

            log_error("pipeline commands are too many");

            return MiniShell_ERR_UNKNOWN;
        }

        if (current->next && pipe(pipefd) < 0) {
            int saved_errno = errno;

            if (pipe_fd >= 0)
                close(pipe_fd);

            close(gate[0]);
            close(gate[1]);

            rollback_pipeline(pgid,
                              pids,
                              count);

            log_error("pipe create failed: %s",
                      strerror(saved_errno));

            return MiniShell_ERR_PIPE;
        }

        pid_t pid = fork();

        if (pid < 0) {
            int saved_errno = errno;

            if (pipefd[0] >= 0)
                close(pipefd[0]);

            if (pipefd[1] >= 0)
                close(pipefd[1]);

            if (pipe_fd >= 0)
                close(pipe_fd);

            close(gate[0]);
            close(gate[1]);

            rollback_pipeline(pgid,
                              pids,
                              count);

            log_error("fork failed: %s",
                      strerror(saved_errno));

            return MiniShell_ERR_FORK;
        }

        if (pid == 0) {
            pid_t child_pgid =
                pgid == 0
                    ? getpid()
                    : pgid;

            child_prepare_before_gate(gate[0],
                                      gate[1],
                                      child_pgid,
                                      ctx);

            if (pipe_fd >= 0 &&
                redirect_fd(pipe_fd,
                            STDIN_FILENO) != MiniShell_OK)
                _exit(1);

            if (current->next &&
                redirect_fd(pipefd[1],
                            STDOUT_FILENO) != MiniShell_OK)
                _exit(1);

            if (pipefd[0] >= 0)
                close(pipefd[0]);

            if (setredirect(current) != MiniShell_OK)
                _exit(1);

            run_process(current);
        }

        pids[count++] = pid;

        if (pgid == 0)
            pgid = pid;

        if (setpgid_parent(pid, pgid) < 0) {
            int saved_errno = errno;

            if (pipefd[0] >= 0)
                close(pipefd[0]);

            if (pipefd[1] >= 0)
                close(pipefd[1]);

            if (pipe_fd >= 0)
                close(pipe_fd);

            close(gate[0]);
            close(gate[1]);

            rollback_pipeline(pgid,
                              pids,
                              count);

            log_error("failed setpgid for pipeline child %d: %s",
                      pid,
                      strerror(saved_errno));

            return MiniShell_ERR_UNKNOWN;
        }

        if (pipe_fd >= 0)
            close(pipe_fd);

        pipe_fd = -1;

        if (current->next) {
            close(pipefd[1]);
            pipe_fd = pipefd[0];
        }

        current = current->next;
    }

    if (pipe_fd >= 0)
        close(pipe_fd);

    close(gate[0]);

    Job *job = job_add(&ctx->jobs,
                       pgid,
                       com->argv[0]);

    if (!job) {
        close(gate[1]);

        rollback_pipeline(pgid,
                          pids,
                          count);

        return MiniShell_ERR_JOB;
    }

    for (int i = 0; i < count; i++) {
        if (process_add(job, pids[i]) < 0) {
            close(gate[1]);

            rollback_pipeline(pgid,
                              pids,
                              count);

            job_remove(&ctx->jobs,
                       pgid);

            return MiniShell_ERR_JOB;
        }
    }

    if (!com->background &&
        terminal_active &&
        terminal_set_foreground(pgid) < 0) {
        int saved_errno = errno;

        close(gate[1]);

        rollback_pipeline(pgid,
                          pids,
                          count);

        job_remove(&ctx->jobs,
                   pgid);

        terminal_restore();

        log_error("failed set pipeline foreground pgid %d: %s",
                  pgid,
                  strerror(saved_errno));

        return MiniShell_ERR_UNKNOWN;
    }

    if (startup_gate_release(gate[1], count) < 0) {
        close(gate[1]);

        rollback_pipeline(pgid,
                          pids,
                          count);

        job_remove(&ctx->jobs,
                   pgid);

        if (!com->background && terminal_active)
            terminal_restore();

        return MiniShell_ERR_UNKNOWN;
    }

    close(gate[1]);

    if (com->background)
        return MiniShell_OK;

    if (job_wait_foreground(job) < 0) {
        rollback_pipeline(pgid,
                          pids,
                          count);

        if (terminal_active)
            terminal_restore();

        job_remove(&ctx->jobs,
                   pgid);

        return MiniShell_ERR_UNKNOWN;
    }

    return finish_foreground_job(ctx,
                                 job,
                                 terminal_active);
}

