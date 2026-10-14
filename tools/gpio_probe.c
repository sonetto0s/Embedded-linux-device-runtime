#include "gpio_line.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void print_usage(const char *program)
{
    fprintf(stderr,
            "Usage:\n"
            "  %s input <gpiochip> <offset> [default|up|down|disabled]\n"
            "  %s watch <gpiochip> <offset> [default|up|down|disabled]\n"
            "  %s output <gpiochip> <offset> <0|1>\n",
            program, program, program);
}

static int parse_offset(const char *text, unsigned int *offset)
{
    if (!text || !offset || !*text)
    {
        return -1;
    }

    errno = 0;

    char *end = NULL;
    unsigned long value = strtoul(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0' || value > UINT_MAX)
    {
        return -1;
    }

    *offset = (unsigned int)value;
    return 0;
}

static int parse_value(const char *text, int *value)
{
    if (!text || !value)
    {
        return -1;
    }

    if (strcmp(text, "0") == 0)
    {
        *value = 0;
        return 0;
    }

    if (strcmp(text, "1") == 0)
    {
        *value = 1;
        return 0;
    }

    return -1;
}

static int parse_bias(const char *text, GpioBias *bias)
{
    if (!text || !bias)
    {
        return -1;
    }

    if (strcmp(text, "default") == 0)
    {
        *bias = GPIO_BIAS_DEFAULT;
        return 0;
    }

    if (strcmp(text, "up") == 0)
    {
        *bias = GPIO_BIAS_PULL_UP;
        return 0;
    }

    if (strcmp(text, "down") == 0)
    {
        *bias = GPIO_BIAS_PULL_DOWN;
        return 0;
    }

    if (strcmp(text, "disabled") == 0)
    {
        *bias = GPIO_BIAS_DISABLED;
        return 0;
    }

    return -1;
}

static int run_input(const char *chip_path, unsigned int offset, GpioBias bias)
{
    GpioLine line;
    gpio_line_init(&line);

    if (gpio_line_open_input(&line, chip_path, offset, bias) < 0)
    {
        perror("gpio_line_open_input");
        return 1;
    }

    int value;

    if (gpio_line_get_value(&line, &value) < 0)
    {
        perror("gpio_line_get_value");
        gpio_line_close(&line);
        return 1;
    }

    printf("%d\n", value);

    gpio_line_close(&line);
    return 0;
}

static int run_watch(const char *chip_path, unsigned int offset, GpioBias bias)
{
    GpioLine line;
    gpio_line_init(&line);

    if (gpio_line_open_input(&line, chip_path, offset, bias) < 0)
    {
        perror("gpio_line_open_input");
        return 1;
    }

    int previous;

    if (gpio_line_get_value(&line, &previous) < 0)
    {
        perror("gpio_line_get_value");
        gpio_line_close(&line);
        return 1;
    }

    printf("value=%d\n", previous);
    fflush(stdout);

    const struct timespec interval =
    {
        .tv_sec = 0,
        .tv_nsec = 50000000L
    };

    for (;;)
    {
        struct timespec remaining = interval;

        while (nanosleep(&remaining, &remaining) < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            perror("nanosleep");
            gpio_line_close(&line);
            return 1;
        }

        int value;

        if (gpio_line_get_value(&line, &value) < 0)
        {
            perror("gpio_line_get_value");
            gpio_line_close(&line);
            return 1;
        }

        if (value != previous)
        {
            printf("value=%d\n", value);
            fflush(stdout);

            previous = value;
        }
    }
}

static int run_output(const char *chip_path, unsigned int offset, int value)
{
    GpioLine line;
    gpio_line_init(&line);

    if (gpio_line_open_output(&line, chip_path, offset, value) < 0)
    {
        perror("gpio_line_open_output");
        return 1;
    }

    printf("value=%d\n", value);

    gpio_line_close(&line);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        print_usage(argv[0]);
        return 2;
    }

    const char *mode = argv[1];
    const char *chip_path = argv[2];

    unsigned int offset;

    if (parse_offset(argv[3], &offset) < 0)
    {
        fprintf(stderr, "Invalid GPIO offset: %s\n", argv[3]);
        return 2;
    }

    if (strcmp(mode, "input") == 0 || strcmp(mode, "watch") == 0)
    {
        if (argc > 5)
        {
            print_usage(argv[0]);
            return 2;
        }

        GpioBias bias = GPIO_BIAS_DEFAULT;

        if (argc == 5 && parse_bias(argv[4], &bias) < 0)
        {
            fprintf(stderr, "Invalid GPIO bias: %s\n", argv[4]);
            return 2;
        }

        if (strcmp(mode, "input") == 0)
        {
            return run_input(chip_path, offset, bias);
        }

        return run_watch(chip_path, offset, bias);
    }

    if (strcmp(mode, "output") == 0)
    {
        if (argc != 5)
        {
            print_usage(argv[0]);
            return 2;
        }

        int value;

        if (parse_value(argv[4], &value) < 0)
        {
            fprintf(stderr, "Invalid GPIO value: %s\n", argv[4]);
            return 2;
        }

        return run_output(chip_path, offset, value);
    }

    print_usage(argv[0]);
    return 2;
}

