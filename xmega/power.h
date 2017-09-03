// Copyright 2017 jem@seethis.link
// Licensed under the MIT license (http://opensource.org/licenses/MIT)

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MODE_BATTERY,
    MODE_WIRED,
    MODE_USB,
    MODE_INIT,
} power_mode_t;

void set_power_mode(power_mode_t mode);
power_mode_t get_power_mode(void);

void vbus_pin_init(void);
bool has_vbus_power(void);
void check_pins_init(void);

void battery_mode_clock_init(void);
void enter_sleep_mode(uint8_t sleep_mode);
void deep_sleep(void);
