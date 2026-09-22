#include "builtin_table.h"
#include "builtin.h"
#include "test_framework.h"

typedef struct
{
    const char *name;
    Builtinhandler handler;
} ExpectedBuiltin;

static const ExpectedBuiltin expected_builtins[] = {
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

void test_builtin_count(void)
{
    size_t expected = sizeof(expected_builtins) / sizeof(expected_builtins[0]);

    TEST_ASSERT_EQ(builtin_count(), expected);
}

void test_builtin_lookup_known(void)
{
    size_t count = sizeof(expected_builtins) / sizeof(expected_builtins[0]);

    for (size_t i = 0; i < count; i++)
    {
        BuiltinEntry *entry = builtin_lookup(expected_builtins[i].name);

        TEST_ASSERT_NOT_NULL(entry);

        if (entry)
        {
            TEST_ASSERT_STR_EQ(entry->name, expected_builtins[i].name);
            TEST_ASSERT(entry->handler == expected_builtins[i].handler);
        }
    }
}

void test_builtin_lookup_unknown(void)
{
    BuiltinEntry *entry = builtin_lookup("not_a_builtin");

    TEST_ASSERT_NULL(entry);
}

void test_builtin_get(void)
{
    size_t count = sizeof(expected_builtins) / sizeof(expected_builtins[0]);

    for (size_t i = 0; i < count; i++)
    {
        BuiltinEntry *entry = builtin_get(i);

        TEST_ASSERT_NOT_NULL(entry);

        if (entry)
        {
            TEST_ASSERT_STR_EQ(entry->name, expected_builtins[i].name);
            TEST_ASSERT(entry->handler == expected_builtins[i].handler);
        }
    }

    TEST_ASSERT_NULL(builtin_get(count));
}





