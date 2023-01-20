// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2022, Input Labs Oy.

#include <stdio.h>
#include <driver/gpio.h>
#include <driver/i2c.h>
#include <driver/spi_master.h>
#include <driver/spi_slave.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include "headers/bus.h"
#include "headers/pin.h"
#include "headers/helper.h"

void bus_i2c_init() {
    i2c_set_pin(I2C_NUM_0, PIN_SDA, PIN_SCL, 1, 1, I2C_MODE_MASTER);
    
    if (!gpio_get_level(PIN_SDA) || !gpio_get_level(PIN_SCL)) {
        printf("ERROR: I2C bus is not clean\n");
        vTaskDelay(1000000000 / portTICK_PERIOD_MS);
    }
}

int8_t bus_i2c_acknowledge(uint8_t device) {
    uint8_t buf = 0;
    return i2c_master_read_from_device(I2C_NUM_0, device, &buf, 1, 0);
}

void bus_i2c_read(uint8_t device, uint8_t reg, uint8_t *buf, uint8_t len) {
    i2c_master_write_read_device(I2C_NUM_0, device, &reg, 1, buf, len, 0);
}

uint8_t bus_i2c_read_one(uint8_t device, uint8_t reg) {
    uint8_t buf[1] = {0};
    bus_i2c_read(device, reg, buf, 1);
    return buf[0];
}

uint16_t bus_i2c_read_two(uint8_t device, uint8_t reg) {
    uint16_t buf[1] = {0};
    bus_i2c_read(device, reg, (uint8_t*)&buf, 2);
    return buf[0];
}

void bus_i2c_write(uint8_t device, uint8_t reg, uint8_t value) {
    uint8_t data[] = {reg, value};
    i2c_master_write_to_device(I2C_NUM_0, device, data, 2, 0);
}

void bus_i2c_io_init() {
    printf("Config I2C IO\n");
    bus_i2c_write(I2C_IO_0, I2C_IO_REG_POLARITY,   0b11111111);
    bus_i2c_write(I2C_IO_0, I2C_IO_REG_POLARITY+1, 0b11111111);
    bus_i2c_write(I2C_IO_1, I2C_IO_REG_POLARITY,   0b11111111);
    bus_i2c_write(I2C_IO_1, I2C_IO_REG_POLARITY+1, 0b11111111);
    bus_i2c_write(I2C_IO_0, I2C_IO_REG_PULLUP,     0b11111111);
    bus_i2c_write(I2C_IO_0, I2C_IO_REG_PULLUP+1,   0b11111111);
    bus_i2c_write(I2C_IO_1, I2C_IO_REG_PULLUP,     0b11111111);
    bus_i2c_write(I2C_IO_1, I2C_IO_REG_PULLUP+1,   0b11111111);
    printf("  I2C_IO_0 ");
    printf("ack=%i ", bus_i2c_acknowledge(I2C_IO_0));
    printf("polarity=%li ", bin(bus_i2c_read_one(I2C_IO_0, I2C_IO_REG_POLARITY)));
    printf("pullup=%li\n", bin(bus_i2c_read_one(I2C_IO_0, I2C_IO_REG_PULLUP)));
    printf("  I2C_IO_1 ");
    printf("ack=%i ", bus_i2c_acknowledge(I2C_IO_1));
    printf("polarity=%li ", bin(bus_i2c_read_one(I2C_IO_1, I2C_IO_REG_POLARITY)));
    printf("pullup=%li\n", bin(bus_i2c_read_one(I2C_IO_1, I2C_IO_REG_PULLUP)));
}

uint16_t io_cache_0;
uint16_t io_cache_1;

void bus_i2c_io_update_cache() {
    io_cache_0 = bus_i2c_read_two(I2C_IO_0, I2C_IO_REG_INPUT);
    io_cache_1 = bus_i2c_read_two(I2C_IO_1, I2C_IO_REG_INPUT);
}

uint16_t bus_i2c_io_get_cache(uint8_t index) {
    return index ? io_cache_1 : io_cache_0;
}

spi_bus_config_t bus_config = {
    .mosi_io_num = PIN_SPI_TX,
    .miso_io_num = PIN_SPI_RX,
    .sclk_io_num = PIN_SPI_CK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1
};
spi_device_handle_t imu_spi;
spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = SPI_FREQ,
    .command_bits=0,
    .address_bits=0,
    .dummy_bits=0,
    .mode=0,
    .spics_io_num = PIN_SPI_CS0
};


void bus_spi_init() {
    printf("Config SPI bus\n");
    spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO);
    spi_bus_add_device(SPI2_HOST, &dev_cfg, &imu_spi);
    gpio_set_direction(PIN_SPI_CS0, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_SPI_CS1, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_SPI_CS0, 1);
    gpio_set_level(PIN_SPI_CS1, 1);
}

void bus_spi_write(uint8_t cs, uint8_t reg, uint8_t value) {
    gpio_set_level(cs, 0);
    uint8_t buf[] = {reg, value};
    spi_transaction_t trans = {
        .tx_buffer = buf,
        .length = 2 * 8,
    };
    spi_device_transmit(imu_spi, &trans);
    gpio_set_level(cs, 1);
}

void bus_spi_read(uint8_t cs, uint8_t reg, uint8_t *buf, uint8_t size) {
    gpio_set_level(cs, 0);
    reg |= 0b10000000;  // Read byte.
    spi_transaction_t trans = {
        .tx_buffer = &reg,
        .length = 1 * 8,
    };
    spi_device_transmit(imu_spi, &trans); //write
    spi_transaction_t rec = {
        .rx_buffer = buf,
        .length = (size * 8),
    };
    spi_device_transmit(imu_spi, &rec); //read
    gpio_set_level(cs, 1);
}

uint8_t bus_spi_read_one(uint8_t cs, uint8_t reg) {
    uint8_t buf[1] = {0};
    bus_spi_read(cs, reg, buf, 1);
    return buf[0];
}

void bus_init() {
    bus_i2c_init();
    bus_i2c_io_init();
    bus_spi_init();
}
