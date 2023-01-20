// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2022, Input Labs Oy.

#pragma once
#include "button.h"
#include <driver/adc.h>

#define ANALOG_FACTOR 32767
#define TRIGGER_FACTOR 255

typedef struct Thumbstick_struct Thumbstick;

struct Thumbstick_struct {
    void (*report) (Thumbstick *self);
    void (*reset) (Thumbstick *self);
    Button left;
    Button right;
    Button up;
    Button down;
    Button push;
    Button inner;
    Button outer;
};

Thumbstick Thumbstick_ (
    Button up,
    Button left,
    Button right,
    Button down,
    Button push,
    Button inner,
    Button outer
);

void thumbstick_init();
void thumbstick_report();
void thumbstick_calibrate();
void thumbstick_update_deadzone();