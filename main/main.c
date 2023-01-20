// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2022, Input Labs Oy.

#include <stdio.h>
#include <stdlib.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <tinyusb.h>
#include <driver/i2c.h>
#include "config.h"
#include "led.h"
#include "bus.h"
#include "profile.h"
#include "touch.h"
#include "imu.h"
#include "hid.h"
#include "uart.h"

void stdio_init() {
    //uart_driver_install(UART_NUM_0, UART_FIFO_LEN, 0, 0, NULL, 0);
}

void title() {
    printf("\n");
    printf("╔═══════════════════════════╗\n");
    printf("║ Input Labs Oy.            ║\n");
    printf("║ Alpakka - firmware 0.86.4 ║\n");
    printf("╚═══════════════════════════╝\n");
}

void main_init() {
    stdio_init();
    title();
    config_init();
    led_init();
    bus_init();
    thumbstick_init();
    touch_init();
    rotary_init();
    profile_init();
    imu_init();
    tusb_init();
}

void main_loop() {
    int16_t i = 0;
    while (true) {
        // Start timer.
        uint32_t tick_start = (uint32_t)esp_timer_get_time();
        // Report.
        profile_report_active();
        hid_report();
        // Tick interval control.
        uint32_t tick_completed = (uint32_t)esp_timer_get_time() - tick_start;
        uint16_t tick_interval = 1000000 / CFG_TICK_FREQUENCY;
        int32_t tick_idle = tick_interval - (int32_t)tick_completed;
        // Listen to incomming UART messages.
        if (!(i % CFG_TICK_FREQUENCY)) {
            uart_listen_char();
        }
        // Print additional timing data.
        if (CFG_LOG_LEVEL && !(i % 1000)) {
            printf("Tick comp=%li idle=%li\n", tick_completed, tick_idle);
        }
        if (tick_idle > 0) {
            vTaskDelay((uint32_t)tick_idle / 10000);
        } else {
            printf("+");
        }
        i++;
    }
}

void app_main(void) {
    main_init();
    main_loop();
}

