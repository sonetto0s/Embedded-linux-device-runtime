#include "watchdog_device.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static volatile sig_atomic_t stop_requested;

static void handle_signal(int signo)
{
    (void)signo;
    stop_requested = 1;
}

static int install_signal_handlers(void)
{
    struct sigaction action;
    memset(&action, 0, sizeof(action));

    action.sa_handler = handle_signal;

    if (sigemptyset(&action.sa_mask) < 0)
    {
        return -1;
    }

    if (sigaction(SIGINT, &action, NULL) < 0)
    {
        return -1;
    }

    if (sigaction(SIGTERM, &action, NULL) < 0)
    {
        return -1;
    }

    return 0;
}

static int sleep_seconds(int seconds)
{
    struct timespec request;
    request.tv_sec = seconds;
    request.tv_nsec = 0;

    while (nanosleep(&request, &request) < 0)
    {
        if (errno != EINTR)
        {
            return -1;
        }

        if (stop_requested)
        {
            return 0;
        }
    }

    return 0;
}

static int parse_positive_int(const char *text, int *value)
{
    if (!text || !value)
    {
        return -1;
    }

    errno = 0;

    char *end = NULL;
    long parsed = strtol(text, &end, 10);

    if (errno != 0 || !end || *end != '\0' || parsed <= 0 || parsed > 86400)
    {
        return -1;
    }

    *value = (int)parsed;
    return 0;
}

static void print_options(unsigned int options)
{
    printf("Options       : 0x%08x\n", options);
    printf("  SETTIMEOUT  : %s\n", options & WDIOF_SETTIMEOUT ? "yes" : "no");
    printf("  MAGICCLOSE  : %s\n", options & WDIOF_MAGICCLOSE ? "yes" : "no");
    printf("  KEEPALIVE   : %s\n", options & WDIOF_KEEPALIVEPING ? "yes" : "no");
    printf("  PRETIMEOUT  : %s\n", options & WDIOF_PRETIMEOUT ? "yes" : "no");
}

static void print_device_info(const WatchdogDevice *device)
{
    printf("\n========== Watchdog ==========\n");
    printf("Identity      : %s\n", device->info.identity);
    printf("Firmware      : %u\n", device->info.firmware_version);
    printf("Timeout       : %d s\n", device->timeout);

    print_options(device->info.options);

    int timeleft;

    if (watchdog_device_get_timeleft((WatchdogDevice *)device, &timeleft) == 0)
    {
        printf("Time Left     : %d s\n", timeleft);
    }
    else
    {
        printf("Time Left     : N/A\n");
    }

    printf("==============================\n\n");
}

static int run_info(WatchdogDevice *device)
{
    print_device_info(device);
    return 0;
}

static int run_keepalive(WatchdogDevice *device, int timeout, int duration)
{
    if (!(device->info.options & WDIOF_SETTIMEOUT))
    {
        fprintf(stderr, "watchdog does not support timeout configuration\n");
        return -1;
    }

    if (watchdog_device_set_timeout(device, timeout) < 0)
    {
        perror("watchdog_device_set_timeout");
        return -1;
    }

    printf("Requested timeout : %d s\n", timeout);
    printf("Actual timeout    : %d s\n", device->timeout);
    printf("Duration          : %d s\n\n", duration);

    int elapsed = 0;

    while (!stop_requested && elapsed < duration)
    {
        if (watchdog_device_keepalive(device) < 0)
        {
            perror("watchdog_device_keepalive");
            return -1;
        }

        int timeleft;

        if (watchdog_device_get_timeleft(device, &timeleft) == 0)
        {
            printf("keep alive: elapsed=%d s timeleft=%d s\n", elapsed, timeleft);
        }
        else
        {
            printf("keep alive: elapsed=%d s\n", elapsed);
        }

        if (sleep_seconds(1) < 0)
        {
            perror("nanosleep");
            return -1;
        }

        elapsed++;
    }

    return 0;
}

static void usage(const char *program)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s <device> info\n", program);
    fprintf(stderr, "  %s <device> keepalive <timeout> <duration>\n", program);
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (install_signal_handlers() < 0)
    {
        perror("install_signal_handlers");
        return EXIT_FAILURE;
    }

    WatchdogDevice device;
    watchdog_device_init(&device);

    if (watchdog_device_open(&device, argv[1]) < 0)
    {
        fprintf(stderr, "Failed to open %s: %s\n", argv[1], strerror(errno));
        return EXIT_FAILURE;
    }

    int result = -1;

    if (strcmp(argv[2], "info") == 0)
    {
        if (argc != 3)
        {
            usage(argv[0]);
        }
        else
        {
            result = run_info(&device);
        }
    }
    else if (strcmp(argv[2], "keepalive") == 0)
    {
        if (argc != 5)
        {
            usage(argv[0]);
        }
        else
        {
            int timeout;
            int duration;

            if (parse_positive_int(argv[3], &timeout) < 0 ||
                parse_positive_int(argv[4], &duration) < 0)
            {
                fprintf(stderr, "invalid timeout or duration\n");
            }
            else
            {
                result = run_keepalive(&device, timeout, duration);
            }
        }
    }
    else
    {
        usage(argv[0]);
    }

    if (watchdog_device_close(&device) < 0)
    {
        perror("watchdog_device_close");
        result = -1;
    }

    return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
