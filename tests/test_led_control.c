#include "led_control.h"
#include "error.h"
#include "test_framework.h"
#include <stddef.h>

void test_led_control_invalid_name(void)
{
    TEST_ASSERT(led_control_set_on(NULL) != MiniShell_OK);
    TEST_ASSERT(led_control_set_off("") != MiniShell_OK);
    TEST_ASSERT(led_control_set_on("../test") != MiniShell_OK);
    TEST_ASSERT(led_control_set_off("test/name") != MiniShell_OK);
}

void test_led_control_invalid_trigger(void)
{
    TEST_ASSERT(led_control_set_trigger(NULL, "heartbeat") != MiniShell_OK);
    TEST_ASSERT(led_control_set_trigger("blue_led", NULL) != MiniShell_OK);
    TEST_ASSERT(led_control_set_trigger("blue_led", "") != MiniShell_OK);
    TEST_ASSERT(led_control_set_trigger("blue_led", "../heartbeat") != MiniShell_OK);
    TEST_ASSERT(led_control_set_trigger("blue_led", "bad trigger") != MiniShell_OK);
}


