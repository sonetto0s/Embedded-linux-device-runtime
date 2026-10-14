#include "../examples/stm32_uart_protocol/stm32_uart_protocol.h"
#include "test_framework.h"
#include <string.h>

typedef struct
{
    int led;
    int fail_led;
    int fail_send;
    char output[1024];
    size_t length;
} Stm32Fixture;

static int fixture_set_led(void *context, int enabled)
{
    Stm32Fixture *fixture = context;
    if (fixture->fail_led)
    {
        return -1;
    }
    fixture->led = enabled;
    return 0;
}

static int fixture_get_led(void *context)
{
    return ((Stm32Fixture *)context)->led;
}

static uint32_t fixture_uptime(void *context)
{
    (void)context;
    return UINT32_MAX;
}

static int fixture_send(void *context, const char *data, size_t size)
{
    Stm32Fixture *fixture = context;
    if (fixture->fail_send || fixture->length + size >= sizeof(fixture->output))
    {
        return -1;
    }
    memcpy(fixture->output + fixture->length, data, size);
    fixture->length += size;
    fixture->output[fixture->length] = '\0';
    return 0;
}

static Stm32UartCallbacks fixture_callbacks(Stm32Fixture *fixture)
{
    Stm32UartCallbacks callbacks = {fixture_set_led, fixture_get_led, fixture_uptime, fixture_send,
                                    fixture};
    return callbacks;
}

void test_stm32_protocol_commands(void)
{
    Stm32Fixture fixture = {0};
    Stm32UartCallbacks callbacks = fixture_callbacks(&fixture);
    char reply[128];
    TEST_ASSERT_EQ(stm32_uart_protocol_handle(&callbacks, "PING", reply, sizeof(reply)), 0);
    TEST_ASSERT_STR_EQ(reply, "PONG\n");
    TEST_ASSERT_EQ(stm32_uart_protocol_handle(&callbacks, "LED ON", reply, sizeof(reply)), 0);
    TEST_ASSERT_EQ(fixture.led, 1);
    TEST_ASSERT_STR_EQ(reply, "OK LED=1\n");
    TEST_ASSERT_EQ(stm32_uart_protocol_handle(&callbacks, "STATUS", reply, sizeof(reply)), 0);
    TEST_ASSERT_STR_EQ(reply, "STATUS LED=1 UPTIME_MS=4294967295\n");
    TEST_ASSERT_EQ(stm32_uart_protocol_handle(&callbacks, "LED OFF", reply, sizeof(reply)), 0);
    TEST_ASSERT_EQ(fixture.led, 0);
    TEST_ASSERT_STR_EQ(reply, "OK LED=0\n");
    TEST_ASSERT_EQ(
        stm32_uart_protocol_handle(&callbacks, "HEARTBEAT 4294967295", reply, sizeof(reply)), 0);
    TEST_ASSERT_STR_EQ(reply, "OK HEARTBEAT=4294967295\n");
}

void test_stm32_protocol_input_errors(void)
{
    Stm32Fixture fixture = {0};
    Stm32UartCallbacks callbacks = fixture_callbacks(&fixture);
    char reply[128];
    TEST_ASSERT_EQ(stm32_uart_protocol_handle(&callbacks, "HEARTBEAT -1", reply, sizeof(reply)), 0);
    TEST_ASSERT_STR_EQ(reply, "ERR ARGUMENT\n");
    TEST_ASSERT_EQ(
        stm32_uart_protocol_handle(&callbacks, "HEARTBEAT 4294967296", reply, sizeof(reply)), 0);
    TEST_ASSERT_STR_EQ(reply, "ERR ARGUMENT\n");
    TEST_ASSERT_EQ(stm32_uart_protocol_handle(&callbacks, "REBOOT", reply, sizeof(reply)), 0);
    TEST_ASSERT_STR_EQ(reply, "ERR COMMAND\n");
    TEST_ASSERT_EQ(fixture.led, 0);
    fixture.fail_led = 1;
    TEST_ASSERT_EQ(stm32_uart_protocol_handle(&callbacks, "LED ON", reply, sizeof(reply)), 0);
    TEST_ASSERT_STR_EQ(reply, "ERR DEVICE\n");
    TEST_ASSERT_EQ(fixture.led, 0);
    TEST_ASSERT_EQ(stm32_uart_protocol_handle(&callbacks, "PING", reply, 4), -1);
    TEST_ASSERT_EQ(stm32_uart_protocol_handle(NULL, "PING", reply, sizeof(reply)), -1);
}

void test_stm32_protocol_fragmented_feed(void)
{
    Stm32Fixture fixture = {0};
    Stm32UartCallbacks callbacks = fixture_callbacks(&fixture);
    Stm32UartProtocol protocol;
    TEST_ASSERT_EQ(stm32_uart_protocol_init(&protocol, &callbacks), 0);
    const char *part = "LED ON";
    for (size_t i = 0; part[i]; i++)
    {
        TEST_ASSERT_EQ(stm32_uart_protocol_feed(&protocol, (unsigned char)part[i]), 0);
    }
    TEST_ASSERT_EQ(fixture.length, 0);
    TEST_ASSERT_EQ(fixture.led, 0);
    TEST_ASSERT_EQ(stm32_uart_protocol_feed(&protocol, '\r'), 0);
    TEST_ASSERT_EQ(fixture.length, 0);
    TEST_ASSERT_EQ(stm32_uart_protocol_feed(&protocol, '\n'), 0);
    TEST_ASSERT_EQ(fixture.led, 1);
    TEST_ASSERT_STR_EQ(fixture.output, "OK LED=1\n");
    const char *next = "PING\nSTATUS\r\n";
    for (size_t i = 0; next[i]; i++)
    {
        TEST_ASSERT_EQ(stm32_uart_protocol_feed(&protocol, (unsigned char)next[i]), 0);
    }
    TEST_ASSERT_STR_EQ(fixture.output, "OK LED=1\nPONG\nSTATUS LED=1 UPTIME_MS=4294967295\n");
}

void test_stm32_protocol_overflow_recovers(void)
{
    Stm32Fixture fixture = {0};
    Stm32UartCallbacks callbacks = fixture_callbacks(&fixture);
    Stm32UartProtocol protocol;
    TEST_ASSERT_EQ(stm32_uart_protocol_init(&protocol, &callbacks), 0);
    for (int i = 0; i < STM32_UART_PROTOCOL_LINE_SIZE + 10; i++)
    {
        TEST_ASSERT_EQ(stm32_uart_protocol_feed(&protocol, 'x'), 0);
    }
    TEST_ASSERT_EQ(stm32_uart_protocol_feed(&protocol, '\n'), 0);
    TEST_ASSERT_STR_EQ(fixture.output, "ERR LINE_TOO_LONG\n");
    const char *next = "PING\n";
    for (size_t i = 0; next[i]; i++)
    {
        TEST_ASSERT_EQ(stm32_uart_protocol_feed(&protocol, (unsigned char)next[i]), 0);
    }
    TEST_ASSERT_STR_EQ(fixture.output, "ERR LINE_TOO_LONG\nPONG\n");
    fixture.fail_send = 1;
    for (size_t i = 0; next[i] && next[i] != '\n'; i++)
    {
        TEST_ASSERT_EQ(stm32_uart_protocol_feed(&protocol, (unsigned char)next[i]), 0);
    }
    TEST_ASSERT_EQ(stm32_uart_protocol_feed(&protocol, '\n'), -1);
}
