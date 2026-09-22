#include "test_framework.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int shell_run_script(const char *script, char **output, int *status);

void test_shell_echo(void)
{
    char *output = NULL;
    int status = 0;
    int ret = shell_run_script("echo hello\n", &output, &status);

    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        return;
    }

    TEST_ASSERT_NOT_NULL(output);

    if (!output)
    {
        return;
    }

    TEST_ASSERT(strstr(output, "hello") != NULL);
    TEST_ASSERT(WIFEXITED(status));
    TEST_ASSERT_EQ(WEXITSTATUS(status), 0);

    free(output);
}

void test_shell_pwd(void)
{
    char cwd[PATH_MAX];
    char *output = NULL;
    int status = 0;

    TEST_ASSERT_NOT_NULL(getcwd(cwd, sizeof(cwd)));

    int ret = shell_run_script("pwd\n", &output, &status);

    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        return;
    }

    TEST_ASSERT_NOT_NULL(output);

    if (!output)
    {
        return;
    }

    TEST_ASSERT(strstr(output, cwd) != NULL);
    TEST_ASSERT(WIFEXITED(status));
    TEST_ASSERT_EQ(WEXITSTATUS(status), 0);

    free(output);
}

void test_shell_external_command(void)
{
    char *output = NULL;
    int status = 0;
    int ret = shell_run_script("/bin/echo external\n", &output, &status);

    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        return;
    }

    TEST_ASSERT_NOT_NULL(output);

    if (!output)
    {
        return;
    }

    TEST_ASSERT(strstr(output, "external") != NULL);
    TEST_ASSERT(WIFEXITED(status));
    TEST_ASSERT_EQ(WEXITSTATUS(status), 0);

    free(output);
}

void test_shell_command_not_found(void)
{
    char *output = NULL;
    int status = 0;
    int ret = shell_run_script("__minishell_command_that_does_not_exist__\n", &output, &status);

    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        return;
    }

    TEST_ASSERT_NOT_NULL(output);

    if (!output)
    {
        return;
    }

    TEST_ASSERT(WIFEXITED(status));
    TEST_ASSERT(WEXITSTATUS(status) != 0);

    free(output);
}

void test_shell_monitor(void)
{
    char *output = NULL;
    int status = 0;
    int ret = shell_run_script("monitor\n", &output, &status);

    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        return;
    }

    TEST_ASSERT_NOT_NULL(output);

    if (!output)
    {
        return;
    }

    TEST_ASSERT(strstr(output, "Runtime Monitor") != NULL);
    TEST_ASSERT(strstr(output, "CPU Usage") != NULL);
    TEST_ASSERT(strstr(output, "Memory Usage") != NULL);
    TEST_ASSERT(strstr(output, "Process Count") != NULL);
    TEST_ASSERT(strstr(output, "Uptime") != NULL);
    TEST_ASSERT(WIFEXITED(status));
    TEST_ASSERT_EQ(WEXITSTATUS(status), 0);

    free(output);
}

void test_shell_psinfo(void)
{
    char *output = NULL;
    int status = 0;
    int ret = shell_run_script("psinfo 1\n", &output, &status);

    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        return;
    }

    TEST_ASSERT_NOT_NULL(output);

    if (!output)
    {
        return;
    }

    TEST_ASSERT(strstr(output, "Process Information") != NULL);
    TEST_ASSERT(strstr(output, "PID") != NULL);
    TEST_ASSERT(strstr(output, "Name") != NULL);
    TEST_ASSERT(strstr(output, "State") != NULL);
    TEST_ASSERT(strstr(output, "Threads") != NULL);
    TEST_ASSERT(WIFEXITED(status));
    TEST_ASSERT_EQ(WEXITSTATUS(status), 0);

    free(output);
}

void test_shell_builtin_pipeline_rejected(void)
{
    char *output = NULL;
    int status = 0;
    int ret = shell_run_script("pwd | wc -c\n", &output, &status);

    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        return;
    }

    TEST_ASSERT_NOT_NULL(output);

    if (!output)
    {
        return;
    }

    TEST_ASSERT(strstr(output, "builtin commands in pipelines are not supported") != NULL);
    TEST_ASSERT(WIFEXITED(status));
    TEST_ASSERT_EQ(WEXITSTATUS(status), 1);

    free(output);
}

void test_shell_builtin_background_rejected(void)
{
    char *output = NULL;
    int status = 0;
    int ret = shell_run_script("pwd &\n", &output, &status);

    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        return;
    }

    TEST_ASSERT_NOT_NULL(output);

    if (!output)
    {
        return;
    }

    TEST_ASSERT(strstr(output, "background builtin commands are not supported") != NULL);
    TEST_ASSERT(WIFEXITED(status));
    TEST_ASSERT_EQ(WEXITSTATUS(status), 1);

    free(output);
}



