// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2022, Input Labs Oy.

#pragma once
#include <stdbool.h>

// Type of button behavior.
#define NORMAL 1
#define STICKY 2
#define HOLD_EXCLUSIVE 3
#define HOLD_EXCLUSIVE_LONG 4
#define HOLD_OVERLAP 5
#define HOLD_OVERLAP_EARLY 6

#define SENTINEL 255
#define ACTIONS(...)  __VA_ARGS__, SENTINEL

typedef struct Button_struct Button;

struct Button_struct {
    bool (*is_pressed) (Button *self);
    void (*report) (Button *self);
    void (*reset) (Button *self);
    void (*handle_normal) (Button *self);
    void (*handle_sticky) (Button *self);
    void (*handle_hold_exclusive) (Button *self, uint16_t time);
    void (*handle_hold_overlap) (Button *self);
    void (*handle_hold_overlap_early) (Button *self);
    uint8_t behavior;
    uint8_t pin;
    uint8_t actions[4];
    uint8_t actions_secondary[4];
    bool state;
    bool state_secondary;
    bool virtual_press;
    uint64_t press_timestamp;
    uint64_t hold_timestamp;
};

Button Button_ (
    uint8_t pin,
    uint8_t behavior,
    ...  // Actions.
);
