#include "builtin_table.h"
#include "builtin.h"
#include "test_framework.h"

void test_builtin_count(void)
{
    size_t count = builtin_count();
    TEST_ASSERT_EQ(count, 13);
}

void test_builtin_lookup_known(void)
{
    BuiltinEntry *entry;

    entry = builtin_lookup("cd");
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_STR_EQ(entry->name, "cd");
    TEST_ASSERT(entry->handler == builtin_cd);

    entry = builtin_lookup("pwd");
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_STR_EQ(entry->name, "pwd");
    TEST_ASSERT(entry->handler == builtin_pwd);

    entry = builtin_lookup("sysinfo");
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_STR_EQ(entry->name, "sysinfo");
    TEST_ASSERT(entry->handler == builtin_sysinfo);

    entry = builtin_lookup("dtinfo");
    TEST_ASSERT_NOT_NULL(entry);

    if (entry)
    {
        TEST_ASSERT_STR_EQ(entry->name, "dtinfo");
        TEST_ASSERT(entry->handler == builtin_dtinfo);
    }

    entry = builtin_lookup("hwinfo");
    TEST_ASSERT_NOT_NULL(entry);

    if (entry)
    {
        TEST_ASSERT_STR_EQ(entry->name, "hwinfo");
        TEST_ASSERT(entry->handler == builtin_hwinfo);
    }

    entry = builtin_lookup("led");
    TEST_ASSERT_NOT_NULL(entry);

    if (entry)
    {
        TEST_ASSERT_STR_EQ(entry->name, "led");
        TEST_ASSERT(entry->handler == builtin_led);
    }
}

void test_builtin_lookup_unknown(void)
{
    BuiltinEntry *entry = builtin_lookup("not_a_builtin");
    TEST_ASSERT_NULL(entry);
}

void test_builtin_get(void)
{
    BuiltinEntry *entry = builtin_get(0);
    TEST_ASSERT_NOT_NULL(entry);

    if (entry)
    {
        TEST_ASSERT_STR_EQ(entry->name, "cd");
        TEST_ASSERT(entry->handler == builtin_cd);
    }

    entry = builtin_get(builtin_count() - 1);
    TEST_ASSERT_NOT_NULL(entry);

    entry = builtin_get(builtin_count());
    TEST_ASSERT_NULL(entry);
}



