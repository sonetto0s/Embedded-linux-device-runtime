#include "test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int shell_run_script(const char *script, char **output, int *status);

static void run_device_script(const char *script, int expected_status, const char *first,
                              const char *second)
{
    const char *dir = getenv("MINISHELL_TEST_DIR");
    if (!dir || !*dir)
    {
        dir = "/tmp";
    }
    char path[512];
    int length = snprintf(path, sizeof(path), "%s/shell-device-%ld.conf", dir, (long)getpid());
    TEST_ASSERT(length > 0 && (size_t)length < sizeof(path));
    if (length <= 0 || (size_t)length >= sizeof(path))
    {
        return;
    }
    FILE *fp = fopen(path, "w");
    TEST_ASSERT_NOT_NULL(fp);
    if (!fp)
    {
        return;
    }
    fputs("[uart]\ntype=uart\npath=/__minishell_missing_device__/uart\n"
          "[disabled]\ntype=led\nled_name=__missing_led__\nenabled=0\n",
          fp);
    fclose(fp);
    const char *previous = getenv("MINISHELL_DEVICES_CONFIG");
    char *saved = previous ? strdup(previous) : NULL;
    int env_ret = setenv("MINISHELL_DEVICES_CONFIG", path, 1);
    TEST_ASSERT_EQ(env_ret, 0);
    if (env_ret < 0)
    {
        free(saved);
        unlink(path);
        return;
    }
    char *output = NULL;
    int status = 0;
    int ret = shell_run_script(script, &output, &status);
    if (saved)
    {
        setenv("MINISHELL_DEVICES_CONFIG", saved, 1);
        free(saved);
    }
    else
    {
        unsetenv("MINISHELL_DEVICES_CONFIG");
    }
    unlink(path);
    TEST_ASSERT_EQ(ret, 0);
    if (ret < 0)
    {
        return;
    }
    TEST_ASSERT_NOT_NULL(output);
    if (!output)
    {
        return;
    }
    TEST_ASSERT(strstr(output, first) != NULL);
    if (second)
    {
        TEST_ASSERT(strstr(output, second) != NULL);
    }
    TEST_ASSERT(WIFEXITED(status));
    if (WIFEXITED(status))
    {
        TEST_ASSERT_EQ(WEXITSTATUS(status), expected_status);
    }
    free(output);
}

void test_shell_device_lazy_list_and_reload(void)
{
    run_device_script("device list\ndevice info uart\ncd /\ndevice reload\ndevice list\nexit\n", 0,
                      "Opened         : 0", "devices config reloaded");
}

void test_shell_device_error_recovery(void)
{
    run_device_script("device read uart 2\ndevice open uart\ndevice open disabled\n"
                      "device info uart\necho device_alive\nexit\n",
                      0, "Opened         : 0", "device_alive");
}

void test_shell_device_missing_name_status(void)
{
    run_device_script("device info missing\nexit\n", 1, "device: device not found: missing", NULL);
}
