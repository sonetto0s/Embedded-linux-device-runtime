#include "process_manager.h"
#include "error.h"
#include "test_framework.h"
#include <unistd.h>

void test_process_manager_invalid(void)
{
    ProcessInfo info;
    ProcessInfo *list = NULL;
    size_t count = 0;


    TEST_ASSERT_EQ(process_manager_get(0, &info), MiniShell_ERR_UNKNOWN);

    TEST_ASSERT_EQ(process_manager_get(getpid(), NULL), MiniShell_ERR_UNKNOWN);
    TEST_ASSERT_EQ(process_manager_collect(NULL, &count), MiniShell_ERR_UNKNOWN);
    TEST_ASSERT_EQ(process_manager_collect(&list, NULL), MiniShell_ERR_UNKNOWN);
}

void test_process_manager_current(void)
{
    ProcessInfo info;

    int ret = process_manager_get(getpid(), &info);

    TEST_ASSERT_EQ(ret, MiniShell_OK);
    TEST_ASSERT_EQ(info.pid, getpid());
    TEST_ASSERT(info.name[0] != '\0');
    TEST_ASSERT(info.state != '\0');
    TEST_ASSERT(info.threads > 0);
}



void test_process_manager_collect(void)
{
    ProcessInfo *list = NULL;
    size_t count = 0;

    int ret = process_manager_collect(&list, &count);

    TEST_ASSERT_EQ(ret, MiniShell_OK);
    TEST_ASSERT(count > 0);
    TEST_ASSERT_NOT_NULL(list);

    if (list)
    {
        for (size_t i = 1; i < count; i++)
        {
            TEST_ASSERT(list[i - 1].pid <= list[i].pid);
        }
    }

    process_manager_free(list);
}
