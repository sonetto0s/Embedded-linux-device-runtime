#include "builtin.h"
#include "builtin_table.h"
#include "device_tree.h"
#include "error.h"
#include "hardware_info.h"
#include "led_control.h"
#include "log.h"
#include "shell_context.h"
#include "system_info.h"
#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cmd_runtime.h"

int builtin_cd(Command *cmd, struct ShellContext *ctx)
{
    (void)ctx;

    if (cmd->argc < 2)
    {
        const char *home = getenv("HOME");

        if (!home)
        {
            fprintf(stderr, "cd: HOME not set\n");
            return MiniShell_ERR_UNKNOWN;
        }

        if (chdir(home) < 0)
        {
            perror("cd");
            return MiniShell_ERR_UNKNOWN;
        }

        return MiniShell_OK;
    }

    if (chdir(cmd->argv[1]) < 0)
    {
        perror("cd");
        return MiniShell_ERR_UNKNOWN;
    }

    return MiniShell_OK;
}

int builtin_pwd(Command *cmd, struct ShellContext *ctx)
{
    (void)cmd;
    (void)ctx;

    char *cwd = getcwd(NULL, 0);

    if (!cwd)
    {
        perror("getcwd");
        return MiniShell_ERR_UNKNOWN;
    }

    printf("%s\n", cwd);
    free(cwd);

    return MiniShell_OK;
}

int builtin_exit(Command *cmd, struct ShellContext *ctx)
{
    (void)cmd;

    if (!ctx)
        return MiniShell_ERR_UNKNOWN;

    ctx->running = 0;

    return ctx->last_exit_status;
}

int builtin_help(Command *cmd, struct ShellContext *ctx)
{
    (void)cmd;
    (void)ctx;

    printf("Builtin commands are:\n");

    for (size_t i = 0; i < builtin_count(); i++)
    {
        BuiltinEntry *entry = builtin_get(i);

        if (entry)
            printf(" %s\n", entry->name);
    }

    return MiniShell_OK;
}

int builtin_jobs(Command *cmd, struct ShellContext *ctx)
{
    (void)cmd;

    job_list(&ctx->jobs);
    return MiniShell_OK;
}

int builtin_status(Command *cmd, struct ShellContext *ctx)
{
    (void)cmd;

    printf("%d\n", ctx->last_exit_status);
    return MiniShell_OK;
}

int builtin_sysinfo(Command *cmd, struct ShellContext *ctx)
{
    (void)cmd;
    (void)ctx;

    SystemInfo info;

    int ret = system_info_collect(&info);

    if (ret != MiniShell_OK)
        return ret;

    system_info_print(&info);
    return MiniShell_OK;
}

int builtin_fg(Command *cmd, struct ShellContext *ctx)
{
    (void)cmd;

    Job *job = ctx->jobs.head;

    while (job)
    {
        if (job->status == JOB_RUNNING || job->status == JOB_STOPPED)
            break;

        job = job->next;
    }

    if (!job)
    {
        printf("fg: no job\n");
        return MiniShell_ERR_JOB;
    }

    if (terminal_set_foreground(job->pgid) < 0)
    {
        log_error("failed set foreground");
        return MiniShell_ERR_UNKNOWN;
    }

    if (job->terminal_modes_valid)
    {
        if (terminal_set_modes(&job->terminal_modes) < 0)
        {
            log_error("failed restore job terminal modes");
            terminal_restore();
            return MiniShell_ERR_UNKNOWN;
        }
    }

    if (job->status == JOB_STOPPED)
    {
        if (job_continue(job) < 0)
        {
            terminal_restore();
            return MiniShell_ERR_JOB;
        }
    }

    if (job_wait_foreground(job) < 0)
    {
        terminal_restore();
        return MiniShell_ERR_UNKNOWN;
    }

    if (job->status == JOB_STOPPED)
    {
        if (terminal_get_modes(&job->terminal_modes) == 0)
            job->terminal_modes_valid = 1;
    }

    if (terminal_restore() < 0)
    {
        log_error("failed restore terminal");
        ctx->running = 0;
        return MiniShell_ERR_UNKNOWN;
    }

    if (job->status == JOB_STOPPED)
    {
        printf("\n[%d]+ Stopped %s\n", job->id, job->command);
        return MiniShell_OK;
    }

    if (job->status != JOB_DONE)
        return MiniShell_ERR_UNKNOWN;

    int status = job_exit_status(job);
    pid_t pgid = job->pgid;

    job_remove(&ctx->jobs, pgid);

    return status < 0 ? MiniShell_ERR_UNKNOWN : status;
}

int builtin_bg(Command *cmd, struct ShellContext *ctx)
{
    (void)cmd;

    Job *job = ctx->jobs.head;

    while (job)
    {
        if (job->status == JOB_STOPPED)
            break;

        job = job->next;
    }

    if (!job)
    {
        printf("bg: no stopped job\n");
        return MiniShell_ERR_JOB;
    }

    if (job_continue(job) < 0)
    {
        log_error("failed continue background job");
        return MiniShell_ERR_JOB;
    }

    printf("[%d] %s &\n", job->id, job->command);

    return MiniShell_OK;
}

int builtin_reload(Command *cmd, struct ShellContext *ctx)
{
    (void)cmd;

    int ret = config_load(&ctx->config, ctx->config_file);

    if (ret != MiniShell_OK)
    {
        log_error("failed reload config: %s", ctx->config_file);
        return MiniShell_ERR_UNKNOWN;
    }

    if (ctx->config.debug)
        log_setlevel(LOG_DEBUG);
    else
        log_setlevel(LOG_INFO);

    printf("config reloaded\n");

    return MiniShell_OK;
}

int builtin_dtinfo(Command *cmd, struct ShellContext *ctx)
{
    (void)cmd;
    (void)ctx;

    DeviceTreeInfo info;
    int ret = device_tree_collect(&info);

    if (ret != MiniShell_OK)
    {
        return ret;
    }

    device_tree_print(&info);

    return MiniShell_OK;
}

int builtin_hwinfo(Command *cmd, struct ShellContext *ctx)
{
    (void)cmd;
    (void)ctx;

    HardwareInfo info;
    int ret = hardware_info_collect(&info);

    if (ret != MiniShell_OK)
    {
        return ret;
    }

    hardware_info_print(&info);

    return MiniShell_OK;
}

static void builtin_led_usage(void)
{
    printf("usage:\n");
    printf("  led list\n");
    printf("  led info <name>\n");
    printf("  led on <name>\n");
    printf("  led off <name>\n");
    printf("  led trigger <name> <trigger>\n");
}

static const LedInfo *builtin_led_find(const HardwareInfo *info, const char *name)
{
    if (!info || !name)
    {
        return NULL;
    }

    for (size_t i = 0; i < info->led_count; i++)
    {
        if (strcmp(info->leds[i].name, name) == 0)
        {
            return &info->leds[i];
        }
    }

    return NULL;
}

static int builtin_led_list(void)
{
    HardwareInfo info;
    int ret = hardware_info_collect(&info);

    if (ret != MiniShell_OK)
    {
        return ret;
    }

    if (info.led_count == 0)
    {
        printf("no LEDs found\n");
        return MiniShell_OK;
    }

    for (size_t i = 0; i < info.led_count; i++)
    {
        const LedInfo *led = &info.leds[i];

        printf("%-16s", led->name);

        if (led->has_brightness && led->has_max_brightness)
        {
            printf(" %ld/%ld", led->brightness, led->max_brightness);
        }
        else
        {
            printf(" N/A");
        }

        printf(" trigger=%s\n", led->trigger[0] ? led->trigger : "N/A");
    }

    return MiniShell_OK;
}

static int builtin_led_info(const char *name)
{
    HardwareInfo info;
    int ret = hardware_info_collect(&info);

    if (ret != MiniShell_OK)
    {
        return ret;
    }

    const LedInfo *led = builtin_led_find(&info, name);

    if (!led)
    {
        fprintf(stderr, "led: device not found: %s\n", name);
        return MiniShell_ERR_UNKNOWN;
    }

    printf("Name           : %s\n", led->name);

    if (led->has_brightness)
    {
        printf("Brightness     : %ld\n", led->brightness);
    }
    else
    {
        printf("Brightness     : N/A\n");
    }

    if (led->has_max_brightness)
    {
        printf("Max Brightness : %ld\n", led->max_brightness);
    }
    else
    {
        printf("Max Brightness : N/A\n");
    }

    printf("Trigger        : %s\n", led->trigger[0] ? led->trigger : "N/A");

    return MiniShell_OK;
}

int builtin_led(Command *cmd, struct ShellContext *ctx)
{
    (void)ctx;

    if (!cmd || cmd->argc < 2)
    {
        builtin_led_usage();
        return MiniShell_ERR_PARSE;
    }

    if (strcmp(cmd->argv[1], "list") == 0)
    {
        if (cmd->argc != 2)
        {
            builtin_led_usage();
            return MiniShell_ERR_PARSE;
        }

        return builtin_led_list();
    }

    if (strcmp(cmd->argv[1], "info") == 0)
    {
        if (cmd->argc != 3)
        {
            builtin_led_usage();
            return MiniShell_ERR_PARSE;
        }

        return builtin_led_info(cmd->argv[2]);
    }

    if (strcmp(cmd->argv[1], "on") == 0)
    {
        if (cmd->argc != 3)
        {
            builtin_led_usage();
            return MiniShell_ERR_PARSE;
        }

        int ret = led_control_set_on(cmd->argv[2]);

        if (ret == MiniShell_OK)
        {
            printf("%s: on\n", cmd->argv[2]);
        }

        return ret;
    }

    if (strcmp(cmd->argv[1], "off") == 0)
    {
        if (cmd->argc != 3)
        {
            builtin_led_usage();
            return MiniShell_ERR_PARSE;
        }

        int ret = led_control_set_off(cmd->argv[2]);

        if (ret == MiniShell_OK)
        {
            printf("%s: off\n", cmd->argv[2]);
        }

        return ret;
    }

    if (strcmp(cmd->argv[1], "trigger") == 0)
    {
        if (cmd->argc != 4)
        {
            builtin_led_usage();
            return MiniShell_ERR_PARSE;
        }

        int ret = led_control_set_trigger(cmd->argv[2], cmd->argv[3]);

        if (ret == MiniShell_OK)
        {
            printf("%s: trigger=%s\n", cmd->argv[2], cmd->argv[3]);
        }

        return ret;
    }

    builtin_led_usage();
    return MiniShell_ERR_PARSE;
}


int builtin_monitor(Command *cmd,
                    struct ShellContext *ctx)
{
    return cmd_monitor(cmd, ctx);
}


int builtin_psinfo(Command *cmd,
                   struct ShellContext *ctx)
{
    return cmd_psinfo(cmd, ctx);
}

