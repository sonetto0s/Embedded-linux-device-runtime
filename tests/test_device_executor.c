#include "command.h"
#include "device_config.h"
#include "executor.h"
#include "shell_context.h"
#include "test_framework.h"
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct
{
    int fd;
} ExecutorDevice;

static int executor_device_open(Device *device)
{
    ExecutorDevice *resource = device->context;

    if (resource->fd < 0)
    {
        errno = EBADF;
        return -1;
    }

    return 0;
}

static int executor_device_close(Device *device)
{
    ExecutorDevice *resource = device->context;
    int fd = resource->fd;
    resource->fd = -1;
    return close(fd);
}

static const DeviceOps executor_device_ops = {
    .open = executor_device_open,
    .close = executor_device_close
};

static Command *make_fifo_commands(const char *path, int count)
{
    Command *head = NULL;
    Command *tail = NULL;

    for (int i = 0; i < count; i++)
    {
        Command *cmd = malloc(sizeof(*cmd));

        if (!cmd)
        {
            command_free(head);
            return NULL;
        }

        command_init(cmd);
        cmd->argv[0] = strdup("/bin/cat");
        cmd->argc = 1;
        cmd->redirect.input_file = strdup(path);

        if (!cmd->argv[0] || !cmd->redirect.input_file)
        {
            command_free(cmd);
            command_free(head);
            return NULL;
        }

        cmd->background = 1;

        if (!head)
        {
            head = cmd;
        }
        else
        {
            tail->next = cmd;
        }

        tail = cmd;
    }

    return head;
}

static void assert_children_running(ShellContext *ctx, int expected)
{
    int count = 0;

    for (Job *job = ctx->jobs.head; job; job = job->next)
    {
        for (Process *process = job->processes; process; process = process->next)
        {
            TEST_ASSERT_EQ(waitpid(process->pid, NULL, WNOHANG), 0);
            count++;
        }
    }

    TEST_ASSERT_EQ(count, expected);
}

static void check_devices_closed_before_redirect(int count)
{
    int resource_pipe[2];
    int ret = pipe(resource_pipe);
    TEST_ASSERT_EQ(ret, 0);

    if (ret < 0)
    {
        return;
    }

    char directory[] = "/tmp/minishell_device_executor_XXXXXX";
    char *created = mkdtemp(directory);
    TEST_ASSERT_NOT_NULL(created);

    if (!created)
    {
        close(resource_pipe[0]);
        close(resource_pipe[1]);
        return;
    }

    char fifo_path[128];
    snprintf(fifo_path, sizeof(fifo_path), "%s/input", directory);
    ret = mkfifo(fifo_path, 0600);
    TEST_ASSERT_EQ(ret, 0);

    if (ret < 0)
    {
        rmdir(directory);
        close(resource_pipe[0]);
        close(resource_pipe[1]);
        return;
    }

    ShellContext ctx;
    shell_context_init(&ctx);
    device_manager_destroy(&ctx.devices);

    ExecutorDevice resource = {.fd = resource_pipe[1]};
    DeviceConfig config;
    device_config_init(&config);
    strcpy(config.name, "inherited_resource");
    strcpy(config.path, "/dev/test_inherited_resource");

    ret = device_manager_register(&ctx.devices, &config,
                                  &executor_device_ops, &resource);
    TEST_ASSERT_EQ(ret, 0);

    Device *device = device_manager_find(&ctx.devices, config.name);
    Command *cmd = make_fifo_commands(fifo_path, count);
    TEST_ASSERT_NOT_NULL(device);
    TEST_ASSERT_NOT_NULL(cmd);

    if (ret == 0 && device && cmd)
    {
        ret = device_open(device);
        TEST_ASSERT_EQ(ret, 0);

        if (ret == 0)
        {
            ret = count == 1 ? execute_single(cmd, &ctx)
                             : execute_pipeline(cmd, &ctx);
            TEST_ASSERT_EQ(ret, 0);

            if (ret == 0)
            {
                /* No writer opens this FIFO, so these children cannot exec. */
                assert_children_running(&ctx, count);
                TEST_ASSERT_EQ(device_close(device), 0);

                struct pollfd descriptor = {
                    .fd = resource_pipe[0],
                    .events = POLLIN,
                    .revents = 0
                };

                do
                {
                    ret = poll(&descriptor, 1, 2000);
                }
                while (ret < 0 && errno == EINTR);

                TEST_ASSERT_EQ(ret, 1);
                TEST_ASSERT((descriptor.revents & POLLHUP) != 0);
                assert_children_running(&ctx, count);
            }
        }
    }

    command_free(cmd);
    shell_context_destroy(&ctx);

    if (resource.fd >= 0)
    {
        close(resource.fd);
    }

    close(resource_pipe[0]);
    unlink(fifo_path);
    rmdir(directory);
}

void test_execute_single_closes_devices_before_redirect(void)
{
    check_devices_closed_before_redirect(1);
}

void test_execute_pipeline_closes_devices_before_redirect(void)
{
    check_devices_closed_before_redirect(2);
}
