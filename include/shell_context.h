#ifndef SHELL_CONTEXT_H
#define SHELL_CONTEXT_H

#include "config.h"
#include "device_manager.h"
#include "job.h"
#include <stddef.h>

#define SHELL_CONFIG_PATH_SIZE 4096
#define SHELL_INPUT_SIZE 4096

typedef struct ShellContext {
    int running;
    JobManager jobs;
    int last_exit_status;
    MiniShellConfig config;

    char config_file[SHELL_CONFIG_PATH_SIZE];
    DeviceManager devices;
    char devices_file[SHELL_CONFIG_PATH_SIZE];

    char input_buffer[SHELL_INPUT_SIZE];
    size_t input_length;
    int input_discarding;
} ShellContext;

void shell_context_init(ShellContext *ctx);
void shell_context_destroy(ShellContext *ctx);

#endif
