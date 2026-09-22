#ifndef CMD_RUNTIME_H
#define CMD_RUNTIME_H

#include "command.h"

struct ShellContext;

int cmd_monitor(Command *cmd, struct ShellContext *ctx);
int cmd_psinfo(Command *cmd, struct ShellContext *ctx);

#endif


