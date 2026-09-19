#include "shell_context.h"
#include "error.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int shell_context_copy_config_path(ShellContext *ctx, const char *path)
{
    if (!ctx || !path || !*path)
    {
        return -1;
    }

    int written = snprintf(ctx->config_file, sizeof(ctx->config_file), "%s", path);

    if (written < 0 || (size_t)written >= sizeof(ctx->config_file))
    {
        ctx->config_file[0] = '\0';
        log_error("config path is too long");
        return -1;
    }

    return 0;
}

static int shell_context_set_environment_config_path(ShellContext *ctx, const char *path)
{
    if (path[0] == '/')
    {
        return shell_context_copy_config_path(ctx, path);
    }

    char cwd[SHELL_CONFIG_PATH_SIZE];

    if (!getcwd(cwd, sizeof(cwd)))
    {
        log_error("failed resolve current directory");
        return -1;
    }

    char absolute[SHELL_CONFIG_PATH_SIZE];
    int written = snprintf(absolute, sizeof(absolute), "%s/%s", cwd, path);

    if (written < 0 || (size_t)written >= sizeof(absolute))
    {
        log_error("config path is too long");
        return -1;
    }

    return shell_context_copy_config_path(ctx, absolute);
}

static int shell_context_try_config_path(ShellContext *ctx, const char *directory, const char *relative)
{
    char path[SHELL_CONFIG_PATH_SIZE];
    int written = snprintf(path, sizeof(path), "%s/%s", directory, relative);

    if (written < 0 || (size_t)written >= sizeof(path))
    {
        return -1;
    }

    if (access(path, F_OK) < 0)
    {
        return -1;
    }

    return shell_context_copy_config_path(ctx, path);
}

static int shell_context_set_config_path(ShellContext *ctx)
{
    const char *environment = getenv("MINISHELL_CONFIG");

    if (environment && *environment)
    {
        return shell_context_set_environment_config_path(ctx, environment);
    }

    char executable[SHELL_CONFIG_PATH_SIZE];
    ssize_t length = readlink("/proc/self/exe", executable, sizeof(executable) - 1);

    if (length > 0 && (size_t)length < sizeof(executable) - 1)
    {
        executable[length] = '\0';
        char *slash = strrchr(executable, '/');

        if (slash)
        {
            if (slash == executable)
            {
                slash[1] = '\0';
            }
            else
            {
                *slash = '\0';
            }

            if (shell_context_try_config_path(ctx, executable, "../config/config.conf") == 0)
            {
                return 0;
            }

            if (shell_context_try_config_path(ctx, executable, "config/config.conf") == 0)
            {
                return 0;
            }
        }
    }

    char cwd[SHELL_CONFIG_PATH_SIZE];

    if (getcwd(cwd, sizeof(cwd)) && shell_context_try_config_path(ctx, cwd, "config/config.conf") == 0)
    {
        return 0;
    }

    ctx->config_file[0] = '\0';
    return -1;
}

void shell_context_init(ShellContext *ctx)
{
    if (!ctx)
    {
        return;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->last_exit_status = 0;

    jobmanager_init(&ctx->jobs);
    config_init(&ctx->config);

    if (shell_context_set_config_path(ctx) == 0)
    {
        int ret = config_load(&ctx->config, ctx->config_file);

        if (ret != MiniShell_OK)
        {
            log_info("using default config");
        }
    }
    else
    {
        log_info("config file not found, using default config");
    }

    log_setlevel(ctx->config.debug ? LOG_DEBUG : LOG_INFO);

    ctx->input_length = 0;
    ctx->input_discarding = 0;
    ctx->input_buffer[0] = '\0';
    ctx->running = 1;
}

void shell_context_destroy(ShellContext *ctx)
{
    if (!ctx)
    {
        return;
    }

    ctx->running = 0;
    job_shutdown(&ctx->jobs);
}






