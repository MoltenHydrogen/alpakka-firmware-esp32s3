// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2022, Input Labs Oy.

#include <stdio.h>
#include <stdlib.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include "config.h"
#include "pin.h"
#include "button.h"
#include "thumbstick.h"
#include "helper.h"
#include "hid.h"
#include "led.h"
#define M_PI		3.14159265358979323846
float offset_x = 0;
float offset_y = 0;
float deadzone = 0;

float thumbstick_value(adc1_channel_t adc_index, float offset) {
    float value = (float)adc1_get_raw(adc_index) - 2048;
    value = value / 2048 * CFG_THUMBSTICK_SATURATION;
    return limit_between(value - offset, -1, 1);
}

void thumbstick_update_deadzone() {
    config_nvm_t config;
    config_read(&config);
    float deadzones[3] = {
        CFG_THUMBSTICK_DEADZONE_LOW,
        CFG_THUMBSTICK_DEADZONE_MID,
        CFG_THUMBSTICK_DEADZONE_HIGH
    };
    deadzone = deadzones[config.deadzone];
}

void thumbstick_update_offsets() {
    config_nvm_t config;
    config_read(&config);
    offset_x = config.ts_offset_x;
    offset_y = config.ts_offset_y;
}

void thumbstick_calibrate() {
    printf("Thumbstick: calibrating...\n");
    float x = 0;
    float y = 0;
    uint32_t len = 100000;
    for(uint32_t i=0; i<len; i++) {
        if (!(i % 5000)) led_cycle_step();
        x += thumbstick_value(ADC2_CHANNEL_7, 0.0);
        y += thumbstick_value(ADC2_CHANNEL_9, 0.0);
    }
    x /= len;
    y /= len;
    printf("Thumbstick: calibration x=%f y=%f\n", x, y);
    config_set_thumbstick_offset(x, y);
    thumbstick_update_offsets();
}

void adc_init() {
    adc2_config_channel_atten(ADC2_CHANNEL_7, ADC_ATTEN_DB_11);
    adc2_config_channel_atten(ADC2_CHANNEL_9, ADC_ATTEN_DB_11);
    esp_adc_cal_characteristics_t adc2_chars;
    esp_adc_cal_characterize(ADC_UNIT_2, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, 0, &adc2_chars);
}

void thumbstick_init() {
    printf("Thumbstick: Init...\n");
    adc_init();
    thumbstick_update_offsets();
    thumbstick_update_deadzone();
    printf("Thumbstick: Init OK\n");
}

void Thumbstick__report(Thumbstick *self) {
    float x = thumbstick_value(ADC1_CHANNEL_4, offset_x);
    float y = thumbstick_value(ADC1_CHANNEL_5, offset_y);
    float angle = atan2(x, -y) * (180 / M_PI);
    float radius = sqrt(powf(x, 2) + powf(y, 2));
    radius = limit_between(radius, 0, 1);
    radius = ramp_low(radius, deadzone);
    x = sin(radians(angle)) * radius;
    y = -cos(radians(angle)) * radius;

    if (radius > deadzone) {
        if (radius < CFG_THUMBSTICK_INNER_RADIUS) self->inner.virtual_press = true;
        else self->outer.virtual_press = true;
        if (is_between(angle, -157.5, -22.5)) self->left.virtual_press = true;
        if (is_between(angle, 22.5, 157.5)) self->right.virtual_press = true;
        if (fabs(angle) < 67.5) self->up.virtual_press = true;
        if (fabs(angle) > 112.5) self->down.virtual_press = true;
    }

    self->push.report(&self->push);
    if (offset_x != 0 && offset_y != 0) {
        // Report only if calibrated.
        self->inner.report(&self->inner);
        self->outer.report(&self->outer);
    }

    if (self->left.actions[0] == GAMEPAD_AXIS_LX && self->right.actions[0] == GAMEPAD_AXIS_LX) {
        hid_gamepad_lx(x * ANALOG_FACTOR);
    } else if (self->left.actions[0] == GAMEPAD_AXIS_RX && self->right.actions[0] == GAMEPAD_AXIS_RX) {
        hid_gamepad_rx(x * ANALOG_FACTOR);
    } else {
        if (self->left.actions[0] == GAMEPAD_AXIS_LZ) hid_gamepad_lz(max(0, -x) * TRIGGER_FACTOR);
        if (self->left.actions[0] == GAMEPAD_AXIS_RZ) hid_gamepad_rz(max(0, -x) * TRIGGER_FACTOR);
        if (self->right.actions[0] == GAMEPAD_AXIS_LZ) hid_gamepad_lz(max(0, x) * TRIGGER_FACTOR);
        if (self->right.actions[0] == GAMEPAD_AXIS_RZ) hid_gamepad_rz(max(0, x) * TRIGGER_FACTOR);
    }

    if (self->up.actions[0] == GAMEPAD_AXIS_LY && self->down.actions[0] == GAMEPAD_AXIS_LY) {
        hid_gamepad_ly(y * ANALOG_FACTOR);
    } else if (self->up.actions[0] == GAMEPAD_AXIS_RY && self->down.actions[0] == GAMEPAD_AXIS_RY) {
        hid_gamepad_ry(y * ANALOG_FACTOR);
    } else {
        if (self->up.actions[0] == GAMEPAD_AXIS_LZ) hid_gamepad_lz(max(0, -y) * TRIGGER_FACTOR);
        if (self->up.actions[0] == GAMEPAD_AXIS_RZ) hid_gamepad_rz(max(0, -y) * TRIGGER_FACTOR);
        if (self->down.actions[0] == GAMEPAD_AXIS_LZ) hid_gamepad_lz(max(0, y) * TRIGGER_FACTOR);
        if (self->down.actions[0] == GAMEPAD_AXIS_RZ) hid_gamepad_rz(max(0, y) * TRIGGER_FACTOR);
    }

    if (self->left.actions[0] < GAMEPAD_AXIS_INDEX) self->left.report(&self->left);
    if (self->right.actions[0] < GAMEPAD_AXIS_INDEX) self->right.report(&self->right);
    if (self->up.actions[0] < GAMEPAD_AXIS_INDEX) self->up.report(&self->up);
    if (self->down.actions[0] < GAMEPAD_AXIS_INDEX) self->down.report(&self->down);
}

void Thumbstick__reset(Thumbstick *self) {
    self->left.reset(&self->left);
    self->right.reset(&self->right);
    self->up.reset(&self->up);
    self->down.reset(&self->down);
    self->push.reset(&self->push);
    self->inner.reset(&self->inner);
    self->outer.reset(&self->inner);
}

Thumbstick Thumbstick_ (
    Button left,
    Button right,
    Button up,
    Button down,
    Button push,
    Button inner,
    Button outer
) {
    Thumbstick thumbstick;
    thumbstick.report = Thumbstick__report;
    thumbstick.reset = Thumbstick__reset;
    thumbstick.left = left;
    thumbstick.right = right;
    thumbstick.up = up;
    thumbstick.down = down;
    thumbstick.inner = inner;
    thumbstick.outer = outer;
    thumbstick.push = push;
    return thumbstick;
}
