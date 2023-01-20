// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2022, Input Labs Oy.

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "config.h"
#include "pin.h"
#include "button.h"
#include "rotary.h"
#include "hid.h"

bool rotary_pending = false;
uint32_t rotary_ts = 0;
int32_t rotary_value = 0;
int8_t rotary_increment = 0;

void IRAM_ATTR rotary_callback(unsigned int gpio, uint32_t events) {
    rotary_ts = (uint32_t) esp_timer_get_time;
    rotary_increment = gpio_get_level(PIN_ROTARY_A) ^ gpio_get_level(PIN_ROTARY_B) ? -1 : 1;
    rotary_pending = true;
}

void rotary_init() {
    printf("Config rotary\n");
    gpio_set_direction(PIN_ROTARY_B, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_ROTARY_B, GPIO_PULLUP_ONLY);
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.pin_bit_mask = ((1ULL<<PIN_ROTARY_A) | (1ULL<<PIN_ROTARY_B));
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
    gpio_set_intr_type(PIN_ROTARY_A, GPIO_INTR_POSEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_ROTARY_A, rotary_callback, (void*) PIN_ROTARY_A);
}

void Rotary__report(Rotary *self) {
    if (
        rotary_pending &&
        ((uint32_t)esp_timer_get_time() > (rotary_ts + CFG_MOUSE_WHEEL_DEBOUNCE))
    ) {
        if (rotary_increment > 0) {
            for(uint8_t i=0; i<4; i++) {
                uint8_t action = self->actions_up[i];
                if (action == MOUSE_SCROLL_UP) hid_mouse_wheel(rotary_increment);
                if (action == MOUSE_SCROLL_DOWN) hid_mouse_wheel(-rotary_increment);
            }
            hid_press_multiple(self->actions_up);
            esp_timer_handle_t alarm;
            esp_timer_create_args_t alarm_cfg = {
            .callback = (esp_timer_cb_t)hid_release_multiple_delayed,
            .arg = self->actions_up,
            .dispatch_method = ESP_TIMER_TASK,
            .skip_unhandled_events = false
            };
            esp_timer_create(&alarm_cfg, &alarm);
            esp_timer_start_once(alarm, 100*1000);
        }
        if (rotary_increment < 0) {
            for(uint8_t i=0; i<4; i++) {
                uint8_t action = self->actions_down[i];
                if (action == MOUSE_SCROLL_UP) hid_mouse_wheel(-rotary_increment);
                if (action == MOUSE_SCROLL_DOWN) hid_mouse_wheel(rotary_increment);
            }
            esp_timer_handle_t alarm;
            esp_timer_create_args_t alarm_cfg = {
            .callback = (esp_timer_cb_t)hid_release_multiple_delayed,
            .arg = self->actions_down,
            .dispatch_method = ESP_TIMER_TASK,
            .skip_unhandled_events = false
            };
            esp_timer_create(&alarm_cfg, &alarm);
            esp_timer_start_once(alarm, 100*1000);
        }
        rotary_increment = 0;
        rotary_pending = false;
    }
}

void Rotary__reset(Rotary *self) {
}

Rotary Rotary_ (
    uint8_t useless,
    ...  // Actions
) {
    Rotary rotary;
    rotary.report = Rotary__report;
    rotary.reset = Rotary__reset;
    rotary.actions_up[0] = 0;
    rotary.actions_up[1] = 0;
    rotary.actions_up[2] = 0;
    rotary.actions_up[3] = 0;
    rotary.actions_down[0] = 0;
    rotary.actions_down[1] = 0;
    rotary.actions_down[2] = 0;
    rotary.actions_down[3] = 0;
    va_list va;
    va_start(va, 0);
    for(uint8_t i=0; true; i++) {
        uint8_t value = va_arg(va, int);
        if (value == SENTINEL) break;
        rotary.actions_up[i] = value;
    }
    for(uint8_t i=0; true; i++) {
        uint8_t value = va_arg(va, int);
        if (value == SENTINEL) break;
        rotary.actions_down[i] = value;
    }
    va_end(va);
    return rotary;
}
