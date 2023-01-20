// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2022, Input Labs Oy.

#include <stdio.h>
#include <driver/gpio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <rom/ets_sys.h>
#include <esp_timer.h>
#include "touch.h"
#include "pin.h"

uint32_t sent_time;
uint32_t delta;
bool current = 0;
bool reported = 0;
int8_t repeated = 0;

void touch_init() {
    printf("Config touch: ");
    gpio_set_direction(PIN_TOUCH_OUT, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_TOUCH_IN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_TOUCH_IN, GPIO_PULLUP_DISABLE);
    gpio_set_pull_mode(PIN_TOUCH_IN, GPIO_PULLDOWN_DISABLE);
    printf("completed\n");
}

bool touch_status() {
    // Send low.
    ets_delay_us(CFG_TOUCH_SETTLE);
    sent_time = (uint32_t) esp_timer_get_time;
    gpio_set_level(PIN_TOUCH_OUT, 0);
    while(gpio_get_level(PIN_TOUCH_IN) == 1) {
        if (((uint32_t) esp_timer_get_time - sent_time) > CFG_TOUCH_TIMEOUT) {
            break;
        }
    };
    // Send high.
    ets_delay_us(CFG_TOUCH_SETTLE);
    sent_time = (uint32_t) esp_timer_get_time;
    gpio_set_level(PIN_TOUCH_OUT, 1);
    while(gpio_get_level(PIN_TOUCH_IN) == 0) {
        if (((uint32_t) esp_timer_get_time - sent_time) > CFG_TOUCH_TIMEOUT) {
            break;
        }
    };
    // Determine if surface is being touched.
    delta = (uint32_t) esp_timer_get_time - sent_time;
    current = (delta > CFG_TOUCH_THRESHOLD) || (delta == 0);
    // Only report change on repeated matches.
    if (current == reported) {
        repeated = 0;
    } else {
        repeated++;
        if (repeated >= CFG_TOUCH_SMOOTH) {
            reported = current;
        }
    }
    // printf("%i-", delta);
    // if (reported) printf("%i-", delta);
    return reported;
}
