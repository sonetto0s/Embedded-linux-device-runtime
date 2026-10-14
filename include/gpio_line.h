#ifndef GPIO_LINE_H
#define GPIO_LINE_H

typedef enum
{
    GPIO_BIAS_DEFAULT = 0,
    GPIO_BIAS_PULL_UP,
    GPIO_BIAS_PULL_DOWN,
    GPIO_BIAS_DISABLED
} GpioBias;

typedef struct
{
    int fd;
} GpioLine;

void gpio_line_init(GpioLine *line);
int gpio_line_open_input(GpioLine *line, const char *chip_path, unsigned int offset, GpioBias bias);
int gpio_line_open_output(GpioLine *line, const char *chip_path, unsigned int offset, int initial_value);
int gpio_line_get_value(const GpioLine *line, int *value);
int gpio_line_set_value(const GpioLine *line, int value);
void gpio_line_close(GpioLine *line);

#endif

