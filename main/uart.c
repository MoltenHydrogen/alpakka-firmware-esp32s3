// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2022, Input Labs Oy.

#include <stdio.h>
#include <driver/uart.h>
#include <esp_task_wdt.h>
#include <esp_rom_efuse.h>
#include "config.h"

#define UART_NUM UART_NUM_1

void uart_listen_char() {
    uint8_t data[1];
    char input = 0;
    int len = uart_read_bytes(UART_NUM, data, 1, 0);
    if (len > 0) {
        input = (char)data[0];
    }
    if (input == 'C') {
        printf("UART: Calibrate\n");
        config_calibrate();
    }
    if (input == 'F') {
        printf("UART: Format NVM\n");
        config_write_init();
        config_print();
    }
}
