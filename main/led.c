// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2022, Input Labs Oy.

#include <stdio.h>
#include <stdlib.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/ledc.h>
#include "led.h"
#include "pin.h"
#include "config.h"

esp_timer_handle_t timer;
esp_timer_create_args_t timer_conf = {
    .callback = &led_cycle_step,
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK
};
ledc_timer_config_t ledc_timer;
ledc_channel_config_t ledc_channel[LEDC_CHANNEL_NUM];
uint8_t channel[] = {LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_CHANNEL_2, LEDC_CHANNEL_3};
uint8_t cycle_position;
uint8_t blink_mask;
bool blink_state = false;

void led_set(uint8_t led, bool state) {
    uint8_t pins[] = {PIN_LED_UP, PIN_LED_RIGHT, PIN_LED_DOWN, PIN_LED_LEFT};
    ledc_channel[led].gpio_num = pins[led];
    ledc_channel_config(&ledc_channel[led]);
    ledc_set_duty_and_update(ledc_timer.speed_mode, ledc_channel[led].channel, state ? (255 * CFG_LED_BRIGHTNESS) : 1, 0);
}

void led_shape_all_off_stop(bool stop) {
    if (stop) led_stop();
    led_set(0, false);
    led_set(1, false);
    led_set(2, false);
    led_set(3, false);
}

void led_shape_all_off() {
    led_shape_all_off_stop(true);
}

void led_shape_all_on() {
    led_stop();
    led_set(0, true);
    led_set(1, true);
    led_set(2, true);
    led_set(3, true);
}

void led_mask(uint8_t mask) {
    if (!(0b0001 & mask)) led_set(0, false);
    if (!(0b0010 & mask)) led_set(1, false);
    if (!(0b0100 & mask)) led_set(2, false);
    if (!(0b1000 & mask)) led_set(3, false);
    if (0b0001 & mask) led_set(0, true);
    if (0b0010 & mask) led_set(1, true);
    if (0b0100 & mask) led_set(2, true);
    if (0b1000 & mask) led_set(3, true);
}

void led_cycle_step() {
    if(cycle_position > 3) cycle_position = 0;
    led_shape_all_off_stop(false);
    led_set(cycle_position, true);
    cycle_position += 1;
}

void led_cycle() {
    cycle_position = 0;
    led_stop();
    esp_timer_create(&timer_conf, &timer);
    esp_timer_start_periodic(timer, 100 * 1000);
}

void led_blink_step() {
    if (LED_MASK_UP & blink_mask) led_set(LED_UP, blink_state);
    if (LED_MASK_RIGHT & blink_mask) led_set(LED_RIGHT, blink_state);
    if (LED_MASK_DOWN & blink_mask) led_set(LED_DOWN, blink_state);
    if (LED_MASK_LEFT & blink_mask) led_set(LED_LEFT, blink_state);
    blink_state = !blink_state;
}

void led_blink_mask(uint8_t mask) {
    led_stop();
    if (LED_MASK_UP & mask) led_set(LED_UP, true);
    if (LED_MASK_RIGHT & mask) led_set(LED_RIGHT, true);
    if (LED_MASK_DOWN & mask) led_set(LED_DOWN, true);
    if (LED_MASK_LEFT & mask) led_set(LED_LEFT, true);
    blink_state = false;
    blink_mask = mask;
    esp_timer_create(&timer_conf, &timer);
    esp_timer_start_periodic(timer, 100 * 1000);
}

void led_stop() {
    esp_timer_stop(timer);
}

void led_init_each(uint8_t led) {
    uint8_t pins[] = {PIN_LED_UP, PIN_LED_RIGHT, PIN_LED_DOWN, PIN_LED_LEFT};
    ledc_channel[led].channel = channel[led];
    uint8_t slice_num = ledc_get_duty(ledc_channel[led].speed_mode, channel[led]);
    ledc_set_duty_with_hpoint(LEDC_SPEED_MODE, channel[led], slice_num, 255);
}

void led_init() {
    ledc_timer.duty_resolution = LEDC_TIMER_8_BIT;
    ledc_timer.freq_hz = 5000;
    ledc_timer.speed_mode = LEDC_SPEED_MODE;
    ledc_timer.timer_num = LEDC_TIMER_0;
    ledc_timer_config(&ledc_timer);
    for (int i = 0; i < LEDC_CHANNEL_NUM; i++) {
        ledc_channel[i].duty = 0;
        ledc_channel[i].speed_mode = LEDC_SPEED_MODE;
        ledc_channel[i].timer_sel = LEDC_TIMER_0;
        ledc_channel_config(&ledc_channel[i]);
    }
    // Pico LED.
    gpio_set_direction(PIN_LED_PICO, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_LED_PICO, 1);
    // Front LEDs.
    led_init_each(0);
    led_init_each(1);
    led_init_each(2);
    led_init_each(3);
    led_blink_mask(0b1111);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    led_stop();
}
