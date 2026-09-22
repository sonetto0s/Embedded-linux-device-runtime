#include "process_monitor.h"

#include "error.h"
#include "test_framework.h"
#include <unistd.h>

void test_process_monitor_invalid(void)
{
    ProcessInfo info;
    ProcessInfo *list = NULL;
    size_t count = 0;

    TEST_ASSERT_EQ(process_monitor_get(0, &info), MiniShell_ERR_UNKNOWN);
    TEST_ASSERT_EQ(process_monitor_get(-1, &info), MiniShell_ERR_UNKNOWN);
    TEST_ASSERT_EQ(process_monitor_get(getpid(), NULL), MiniShell_ERR_UNKNOWN);
    TEST_ASSERT_EQ(process_monitor_collect(NULL, &count), MiniShell_ERR_UNKNOWN);
    TEST_ASSERT_EQ(process_monitor_collect(&list, NULL), MiniShell_ERR_UNKNOWN);
}

void test_process_monitor_current(void)
{
    ProcessInfo info;

    int ret = process_monitor_get(getpid(), &info);

    TEST_ASSERT_EQ(ret, MiniShell_OK);
    TEST_ASSERT_EQ(info.pid, getpid());
    TEST_ASSERT(info.name[0] != '\0');
    TEST_ASSERT(info.state != '?');
    TEST_ASSERT(info.threads > 0);
}

void test_process_monitor_collect(void)
{
    ProcessInfo *list = NULL;
    size_t count = 0;

    int ret = process_monitor_collect(&list, &count);

    TEST_ASSERT_EQ(ret, MiniShell_OK);
    TEST_ASSERT(count > 0);
    TEST_ASSERT_NOT_NULL(list);

    if (!list)
    {
        return;
    }

    int current_found = 0;

    for (size_t i = 0; i < count; i++)
    {
        if (list[i].pid == getpid())
        {
            current_found = 1;
        }

        if (i > 0)
        {
            TEST_ASSERT(list[i - 1].pid <= list[i].pid);
        }
    }

    TEST_ASSERT(current_found);

    process_monitor_free(list);
}



