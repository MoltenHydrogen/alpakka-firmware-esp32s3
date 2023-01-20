// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2022, Input Labs Oy.

#include <stdio.h>
#include <esp_flash.h>
#include <esp_flash_internal.h>
#include <freertos/task.h>
#include "helper.h"
#include "config.h"
#include "nvm.h"

static portMUX_TYPE mutex;

void nvm_write(uint32_t addr, uint8_t* buffer, uint32_t size) {
    taskENTER_CRITICAL(&mutex);
    esp_flash_init(NULL);
    esp_flash_erase_region(NULL, addr, max(size, 4096));
    esp_flash_write(NULL, &addr, *buffer, size);
    taskEXIT_CRITICAL(&mutex);
}

void nvm_read(uint32_t addr, uint8_t* buffer, uint32_t size) {
    uint8_t* p = (uint8_t*)(addr);
    for(uint32_t i=0; i<size; i++) {
        buffer[i] = *(p+i);
    }
}
