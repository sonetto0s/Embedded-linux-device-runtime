#include "shell_context.h"
#include "job.h"
#include "test_framework.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void test_shell_context_init(void)
{
    ShellContext ctx;
    shell_context_init(&ctx);
    TEST_ASSERT_EQ(ctx.running, 1);
    TEST_ASSERT_EQ(ctx.last_exit_status, 0);
    TEST_ASSERT_NULL(ctx.jobs.head);
    TEST_ASSERT_EQ(ctx.jobs.nextid, 1);
    TEST_ASSERT(ctx.config.prompts[0] != '\0');
    TEST_ASSERT(ctx.config.max_job > 0);
    shell_context_destroy(&ctx);
}

void test_shell_context_config_env(void)
{
    const char *filename = "/tmp/minishell_context_config.conf";
    FILE *fp = fopen(filename, "w");
    TEST_ASSERT_NOT_NULL(fp);

    if (!fp)
    {
        return;
    }

    fprintf(fp, "prompts=DeployShell\n");
    fprintf(fp, "max_job=32\n");
    fprintf(fp, "debug=1\n");
    fclose(fp);

    int ret = setenv("MINISHELL_CONFIG", filename, 1);
    TEST_ASSERT_EQ(ret, 0);

    if (ret < 0)
    {
        unlink(filename);
        return;
    }

    ShellContext ctx;
    shell_context_init(&ctx);
    TEST_ASSERT_STR_EQ(ctx.config_file, filename);
    TEST_ASSERT_STR_EQ(ctx.config.prompts, "DeployShell");
    TEST_ASSERT_EQ(ctx.config.max_job, 32);
    TEST_ASSERT_EQ(ctx.config.debug, 1);
    shell_context_destroy(&ctx);

    unsetenv("MINISHELL_CONFIG");
    unlink(filename);
}

void test_shell_context_destroy(void)
{
    ShellContext ctx;
    shell_context_init(&ctx);
    TEST_ASSERT_EQ(ctx.running, 1);
    shell_context_destroy(&ctx);
    TEST_ASSERT_EQ(ctx.running, 0);
    TEST_ASSERT_NULL(ctx.jobs.head);
}




void test_shell_context_destroy_reaps_jobs(void)
{
    ShellContext ctx;
    shell_context_init(&ctx);
    pid_t pid = fork();
    TEST_ASSERT(pid >= 0);

    if (pid < 0)
    {
        shell_context_destroy(&ctx);
        return;
    }

    if (pid == 0)
    {
        if (setpgid(0, 0) < 0)
        {
            _exit(1);
        }

        for (;;)
        {
            pause();
        }
    }

    if (setpgid(pid, pid) < 0 && errno != EACCES)
    {
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        shell_context_destroy(&ctx);
        return;
    }

    Job *job = job_add(&ctx.jobs, pid, "context_shutdown_test");
    TEST_ASSERT_NOT_NULL(job);

    if (!job)
    {
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        shell_context_destroy(&ctx);
        return;
    }

    TEST_ASSERT_EQ(process_add(job, pid), 0);
    shell_context_destroy(&ctx);
    TEST_ASSERT_EQ(ctx.running, 0);
    TEST_ASSERT_NULL(ctx.jobs.head);

    errno = 0;

    pid_t ret = waitpid(pid, NULL, WNOHANG);

    TEST_ASSERT_EQ(ret, -1);
    TEST_ASSERT_EQ(errno, ECHILD);
}











