#include "builtin_table.h"
#include "builtin.h"
#include "test_framework.h"

void test_builtin_count(void)
{
    size_t count = builtin_count();
    TEST_ASSERT_EQ(count, 11);
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
}

void test_builtin_lookup_unknown(void)
{
    BuiltinEntry *entry;
    entry = builtin_lookup("not_a_builtin");
    TEST_ASSERT_NULL(entry);
}

void test_builtin_get(void)
{
    BuiltinEntry *entry;
    size_t count = builtin_count();

    entry = builtin_get(0);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_STR_EQ(entry->name, "cd");
    TEST_ASSERT(entry->handler == builtin_cd);

    entry = builtin_get(count - 1);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_STR_EQ(entry->name, "reload");
    TEST_ASSERT(entry->handler == builtin_reload);
    entry = builtin_get(count);
    TEST_ASSERT_NULL(entry);
}

