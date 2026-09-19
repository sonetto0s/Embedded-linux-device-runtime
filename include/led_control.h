#ifndef LED_CONTROL_H
#define LED_CONTROL_H

int led_control_set_on(const char *name);
int led_control_set_off(const char *name);
int led_control_set_trigger(const char *name, const char *trigger);

#endif


