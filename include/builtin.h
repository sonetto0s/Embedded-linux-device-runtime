#ifndef BUILTIN_H
#define BUILTIN_H

#include "command.h"

struct ShellContext;

int builtin_cd(Command *cmd, struct ShellContext *ctx);
int builtin_pwd(Command *cmd, struct ShellContext *ctx);
int builtin_exit(Command *cmd, struct ShellContext *ctx);
int builtin_help(Command *cmd, struct ShellContext *ctx);
int builtin_jobs(Command *cmd, struct ShellContext *ctx);
int builtin_status(Command *cmd, struct ShellContext *ctx);
int builtin_sysinfo(Command *cmd, struct ShellContext *ctx);
int builtin_fg(Command *cmd, struct ShellContext *ctx);
int builtin_bg(Command *cmd, struct ShellContext *ctx);
int builtin_reload(Command *cmd, struct ShellContext *ctx);
int builtin_dtinfo(Command *cmd, struct ShellContext *ctx);
int builtin_hwinfo(Command *cmd, struct ShellContext *ctx);
int builtin_led(Command *cmd, struct ShellContext *ctx);

#endif


