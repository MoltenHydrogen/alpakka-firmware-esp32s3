// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2022, Input Labs Oy.

#include <stdio.h>
#include <stdarg.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include "config.h"
#include "profile.h"
#include "hid.h"
#include "bus.h"

bool Button__is_pressed(Button *self) {
    if (self->pin == 0) {
        if (self->virtual_press) {
            self->virtual_press = false;
            return true;
        } else {
            return false;
        }
    }
    if (self->pin < 100) return !gpio_get_level(self->pin);
    if (self->pin < 200) {
        uint16_t io_cache_0 = bus_i2c_io_get_cache(0);
        return io_cache_0 & (1 << (self->pin - 100));
    }
    else {
        uint16_t io_cache_1 = bus_i2c_io_get_cache(1);
        return io_cache_1 & (1 << (self->pin - 200));
    }
}

void Button__report(Button *self) {
    if (self->behavior == NORMAL) self->handle_normal(self);
    else if (self->behavior == STICKY) self->handle_sticky(self);
    else if (self->behavior == HOLD_OVERLAP) self->handle_hold_overlap(self);
    else if (self->behavior == HOLD_OVERLAP_EARLY) self->handle_hold_overlap_early(self);
    else if (self->behavior == HOLD_EXCLUSIVE) {
        self->handle_hold_exclusive(self, CFG_HOLD_EXCLUSIVE_TIME);
    }
    else if (self->behavior == HOLD_EXCLUSIVE_LONG) {
        self->handle_hold_exclusive(self, CFG_HOLD_EXCLUSIVE_LONG_TIME);
    }
}

void Button__handle_normal(Button *self) {
    if ((uint64_t)esp_timer_get_time < self->press_timestamp + CFG_PRESS_DEBOUNCE * 1000) {
        return;
    };
    bool pressed = self->is_pressed(self);
    if(pressed && !self->state) {
        hid_press_multiple(self->actions);
        self->state = true;
        self->press_timestamp = (uint64_t)esp_timer_get_time;
        return;
    }
    if((!pressed) && self->state) {
        hid_release_multiple(self->actions);
        self->state = false;
        self->press_timestamp = (uint64_t)esp_timer_get_time;
        return;
    }
}

void Button__handle_sticky(Button *self) {
    bool pressed = self->is_pressed(self);
    if(pressed && !self->state) {
        self->state = true;
        hid_press_multiple(self->actions);
        hid_press_multiple(self->actions_secondary);
        return;
    }
    if((!pressed) && self->state) {
        self->state = false;
        hid_release_multiple(self->actions_secondary);
        return;
    }
}
void Button__handle_hold_exclusive(Button *self, uint16_t time) {
    bool pressed = self->is_pressed(self);
    if(pressed && !self->state && !self->state_secondary) {
        self->state = true;
        self->hold_timestamp = (uint64_t)esp_timer_get_time;
        return;
    }
    if(pressed && self->state && !self->state_secondary) {
        uint64_t hold_time_us = time * 1000;
        if ((uint64_t)esp_timer_get_time > self->hold_timestamp + hold_time_us) {
            hid_press_multiple(self->actions_secondary);
            self->state = false;
            self->state_secondary = true;
        }
    }
    if(!pressed) {
        if (self->state) {
            hid_press_multiple(self->actions);
            esp_timer_handle_t alarm;
            esp_timer_create_args_t alarm_cfg = {
            .callback = (esp_timer_cb_t)hid_release_multiple_delayed,
            .arg = self->actions,
            .dispatch_method = ESP_TIMER_TASK,
            .skip_unhandled_events = false
            };
            esp_timer_create(&alarm_cfg, &alarm);
            esp_timer_start_once(alarm, 100*1000);
            self->state = false;
            return;
        }
        if (self->state_secondary) {
            hid_release_multiple(self->actions_secondary);
            self->state_secondary = false;
            return;
        }
    }
}

void Button__handle_hold_overlap(Button *self) {
    bool pressed = self->is_pressed(self);
    if(pressed && !self->state && !self->state_secondary) {
        hid_press_multiple(self->actions);
        self->state = true;
        self->hold_timestamp = (uint64_t)esp_timer_get_time;
        return;
    }
    if(pressed && self->state && !self->state_secondary) {
        uint64_t hold_time_us = CFG_HOLD_OVERLAP_TIME * 1000;
        if ((uint64_t)esp_timer_get_time > self->hold_timestamp + hold_time_us) {
            hid_press_multiple(self->actions_secondary);
            self->state_secondary = true;
        }
        return;
    }
    if(!pressed) {
        if (self->state) {
            hid_release_multiple(self->actions);
            self->state = false;
        }
        if (self->state_secondary) {
            hid_release_multiple(self->actions_secondary);
            self->state_secondary = false;
        }
        return;
    }
}
void Button__handle_hold_overlap_early(Button *self) {
    bool pressed = self->is_pressed(self);
    if(pressed && !self->state && !self->state_secondary) {
        hid_press_multiple(self->actions_secondary);
        self->state_secondary = true;
        self->hold_timestamp = (uint64_t)esp_timer_get_time;
        return;
    }
    if((!pressed) && self->state_secondary) {
        hid_release_multiple(self->actions_secondary);
        uint64_t hold_time_us = CFG_HOLD_OVERLAP_EARLY_TIME * 1000;
        if ((uint64_t)esp_timer_get_time < self->hold_timestamp + hold_time_us) {
            esp_timer_handle_t alarm;
            esp_timer_create_args_t alarm_cfg = {
            .callback = (esp_timer_cb_t)hid_release_multiple_delayed,
            .arg = self->actions,
            .dispatch_method = ESP_TIMER_TASK,
            .skip_unhandled_events = false
            };
            esp_timer_create(&alarm_cfg, &alarm);
            esp_timer_start_once(alarm, 10*1000);
            esp_timer_start_once(alarm, 100*1000);
        }
        self->state_secondary = false;
        return;
    }
}

void Button__reset(Button *self) {
    self->state = false;
}

Button Button_ (
    uint8_t pin,
    uint8_t behavior,
    ...  // Actions.
) {
    if (pin) {
        gpio_set_direction(pin, GPIO_MODE_INPUT);
        gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);
    }
    Button button;
    button.is_pressed = Button__is_pressed;
    button.report = Button__report;
    button.reset = Button__reset;
    button.handle_normal = Button__handle_normal;
    button.handle_sticky = Button__handle_sticky;
    button.handle_hold_exclusive = Button__handle_hold_exclusive;
    button.handle_hold_overlap = Button__handle_hold_overlap;
    button.handle_hold_overlap_early = Button__handle_hold_overlap_early;
    button.pin = pin;
    button.behavior = behavior;
    button.state = false;
    button.virtual_press = false;
    button.state_secondary = false;
    button.press_timestamp = 0;
    button.hold_timestamp = 0;
    button.actions[0] = 0;
    button.actions[1] = 0;
    button.actions[2] = 0;
    button.actions[3] = 0;
    button.actions_secondary[0] = 0;
    button.actions_secondary[1] = 0;
    button.actions_secondary[2] = 0;
    button.actions_secondary[3] = 0;
    // Capture varible arguments.
    va_list va;
    va_start(va, 0);
    for(uint8_t i=0; true; i++) {
        uint8_t value = va_arg(va, int);
        if (value == SENTINEL) break;
        button.actions[i] = value;
    }
    if (button.behavior != NORMAL) {
        for(uint8_t i=0; true; i++) {
            uint8_t value = va_arg(va, int);
            if (value == SENTINEL) break;
            button.actions_secondary[i] = value;
        }
    }
    va_end(va);
    return button;
}
