#include "executor.h"
#include "command.h"
#include "shell_context.h"
#include "test_framework.h"
#include "error.h"
#include "job.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

static Command *make_external_command(const char *name)
{
    Command *cmd = malloc(sizeof(Command));
    if (!cmd) return NULL;
    command_init(cmd);
    cmd->argv[0] = strdup(name);
    if (!cmd->argv[0])
    {
        command_free(cmd);
        return NULL;
    }
    cmd->argc = 1;
    return cmd;
}

static Command *make_sleep_command(void)
{
    Command *cmd = malloc(sizeof(Command));
    if (!cmd) return NULL;
    command_init(cmd);
    cmd->argv[0] = strdup("/bin/sleep");
    if (!cmd->argv[0])
    {
        command_free(cmd);
        return NULL;
    }

    cmd->argv[1] = strdup("5");

    if (!cmd->argv[1])
    {
        command_free(cmd);
        return NULL;
    }

    cmd->argc = 2;
    return cmd;
}

static Command *make_command_chain(const char *name, int count)
{
    Command *head = NULL;
    Command *tail = NULL;

    for (int i = 0; i < count; i++)
    {
        Command *cmd = make_external_command(name);

        if (!cmd)
        {
            command_free(head);
            return NULL;
        }

        if (!head)
            head = cmd;
        else
            tail->next = cmd;

        tail = cmd;
    }

    return head;
}

static Command *make_sleep_chain(int count)
{
    Command *head = NULL;
    Command *tail = NULL;

    for (int i = 0; i < count; i++)
    {
        Command *cmd = make_sleep_command();

        if (!cmd)
        {
            command_free(head);
            return NULL;
        }

        if (!head)
            head = cmd;
        else
            tail->next = cmd;

        tail = cmd;
    }

    return head;
}

static void assert_no_children(void)
{
    errno = 0;
    pid_t ret = waitpid(-1, NULL, WNOHANG);
    TEST_ASSERT_EQ(ret, -1);
    TEST_ASSERT_EQ(errno, ECHILD);
}

void test_execute_single_success_reaps_child(void)
{
    ShellContext ctx;
    shell_context_init(&ctx);
    Command *cmd = make_external_command("/bin/true");
    TEST_ASSERT_NOT_NULL(cmd);
    if (!cmd)
    {
        shell_context_destroy(&ctx);
        return;
    }

    int ret = execute_single(cmd, &ctx);

    TEST_ASSERT_EQ(ret, 0);
    TEST_ASSERT_NULL(ctx.jobs.head);

    assert_no_children();
    command_free(cmd);
    shell_context_destroy(&ctx);
}

void test_execute_single_exec_failure_reaps_child(void)
{
    ShellContext ctx;
    shell_context_init(&ctx);

    Command *cmd = make_external_command("__minishell_executor_command_that_does_not_exist__");

    TEST_ASSERT_NOT_NULL(cmd);

    if (!cmd)
    {
        shell_context_destroy(&ctx);
        return;
    }

    int ret = execute_single(cmd, &ctx);

    TEST_ASSERT_EQ(ret, 127);
    TEST_ASSERT_NULL(ctx.jobs.head);
    assert_no_children();
    command_free(cmd);
    shell_context_destroy(&ctx);
}

void test_execute_pipeline_success_reaps_children(void)
{
    ShellContext ctx;
    shell_context_init(&ctx);
    Command *cmd = make_command_chain("/bin/true", 3);
    TEST_ASSERT_NOT_NULL(cmd);

    if (!cmd)
    {
        shell_context_destroy(&ctx);
        return;
    }

    int ret = execute_pipeline(cmd, &ctx);

    TEST_ASSERT_EQ(ret, 0);
    TEST_ASSERT_NULL(ctx.jobs.head);

    assert_no_children();
    command_free(cmd);
    shell_context_destroy(&ctx);
}

void test_execute_pipeline_exec_failure_reaps_children(void)
{
    ShellContext ctx;
    shell_context_init(&ctx);
    Command *first = make_external_command("/bin/true");
    Command *second = make_external_command("/bin/true");
    Command *third = make_external_command("__minishell_pipeline_command_that_does_not_exist__");
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(second);
    TEST_ASSERT_NOT_NULL(third);

    if (!first || !second || !third)
    {
        command_free(first);
        command_free(second);
        command_free(third);
        shell_context_destroy(&ctx);
        return;
    }

    first->next = second;
    second->next = third;

    int ret = execute_pipeline(first, &ctx);
    TEST_ASSERT_EQ(ret, 127);
    TEST_ASSERT_NULL(ctx.jobs.head);
    assert_no_children();
    command_free(first);
    shell_context_destroy(&ctx);
}

void test_execute_pipeline_limit_cleanup(void)
{
    ShellContext ctx;
    shell_context_init(&ctx);
    Command *cmd = make_sleep_chain(MAX_PIPELINE + 1);
    TEST_ASSERT_NOT_NULL(cmd);

    if (!cmd)
    {
        shell_context_destroy(&ctx);
        return;
    }

    int ret = execute_pipeline(cmd, &ctx);
    TEST_ASSERT_EQ(ret, MiniShell_ERR_UNKNOWN);
    TEST_ASSERT_NULL(ctx.jobs.head);
    assert_no_children();
    command_free(cmd);
    shell_context_destroy(&ctx);
}

void test_execute_command_max_job_limit(void)
{
    ShellContext ctx;
    shell_context_init(&ctx);
    ctx.config.max_job = 1;
    Job *job = job_add(&ctx.jobs, 99999, "fake_active_job");
    TEST_ASSERT_NOT_NULL(job);

    if (!job)
    {
        shell_context_destroy(&ctx);
        return;
    }

    TEST_ASSERT_EQ(job_count_active(&ctx.jobs), 1);
    Command *cmd = make_external_command("/bin/true");
    TEST_ASSERT_NOT_NULL(cmd);

    if (!cmd)
    {
        job_destroy(&ctx.jobs);
        shell_context_destroy(&ctx);
        return;
    }

    int ret = execute_command(cmd, &ctx);
    TEST_ASSERT_EQ(ret, MiniShell_ERR_JOB);
    TEST_ASSERT_EQ(job_count_active(&ctx.jobs), 1);
    assert_no_children();
    command_free(cmd);
    job_destroy(&ctx.jobs);
    shell_context_destroy(&ctx);
}

void test_execute_command_done_job_not_counted(void)
{
    ShellContext ctx;
    shell_context_init(&ctx);
    ctx.config.max_job = 1;
    Job *job = job_add(&ctx.jobs, 99999, "fake_done_job");
    TEST_ASSERT_NOT_NULL(job);

    if (!job)
    {
        shell_context_destroy(&ctx);
        return;
    }

    job->status = JOB_DONE;

    TEST_ASSERT_EQ(job_count_active(&ctx.jobs), 0);
    Command *cmd = make_external_command("/bin/true");
    TEST_ASSERT_NOT_NULL(cmd);

    if (!cmd)
    {
        job_destroy(&ctx.jobs);
        shell_context_destroy(&ctx);
        return;
    }

    int ret = execute_command(cmd, &ctx);
    TEST_ASSERT_EQ(ret, 0);
    command_free(cmd);

    job_destroy(&ctx.jobs);
    shell_context_destroy(&ctx);
    assert_no_children();
}


