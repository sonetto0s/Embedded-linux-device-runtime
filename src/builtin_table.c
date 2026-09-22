#include "builtin_table.h"
#include "builtin.h"
#include <sys/types.h>
#include <string.h>

static BuiltinEntry builtin_table[] = {
    {"cd", builtin_cd},
    {"pwd", builtin_pwd},
    {"exit", builtin_exit},
    {"jobs", builtin_jobs},
    {"help", builtin_help},
    {"status", builtin_status},
    {"sysinfo", builtin_sysinfo},
    {"monitor", builtin_monitor},
    {"psinfo", builtin_psinfo},
    {"dtinfo", builtin_dtinfo},
    {"hwinfo", builtin_hwinfo},
    {"led", builtin_led},
    {"fg", builtin_fg},
    {"bg", builtin_bg},
    {"reload", builtin_reload},
};

BuiltinEntry *builtin_lookup(const char *name)
{
    for (size_t i = 0; i < sizeof(builtin_table) / sizeof(builtin_table[0]); i++)
    {
        if (strcmp(name, builtin_table[i].name) == 0)
            return &builtin_table[i];
    }

    return NULL;
}

BuiltinEntry *builtin_get(size_t index)
{
    if (index >= builtin_count())
        return NULL;

    return &builtin_table[index];
}

size_t builtin_count(void)
{
    return sizeof(builtin_table) / sizeof(builtin_table[0]);
}


















