// Declares the binary UDP input protocol used by the rasta emulator.
// Each keyboard or mouse event is sent as one fixed-size message with
// a simple message id and two integer parameters.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 tomaz stih

#ifndef RASTA_INPUT_PROTOCOL_H
#define RASTA_INPUT_PROTOCOL_H

#include <stdint.h>

enum rasta_msgid {
    rasta_msg_key_down = 1,
    rasta_msg_key_up = 2,
    rasta_msg_mouse_move = 3,

    rasta_msg_mouse_left_down = 10,
    rasta_msg_mouse_left_up = 11,
    rasta_msg_mouse_left_click = 12,
    rasta_msg_mouse_left_double_click = 13,

    rasta_msg_mouse_middle_down = 20,
    rasta_msg_mouse_middle_up = 21,
    rasta_msg_mouse_middle_click = 22,
    rasta_msg_mouse_middle_double_click = 23,

    rasta_msg_mouse_right_down = 30,
    rasta_msg_mouse_right_up = 31,
    rasta_msg_mouse_right_click = 32,
    rasta_msg_mouse_right_double_click = 33,

    rasta_msg_mouse_x1_down = 40,
    rasta_msg_mouse_x1_up = 41,
    rasta_msg_mouse_x1_click = 42,
    rasta_msg_mouse_x1_double_click = 43,

    rasta_msg_mouse_x2_down = 50,
    rasta_msg_mouse_x2_up = 51,
    rasta_msg_mouse_x2_click = 52,
    rasta_msg_mouse_x2_double_click = 53
};

struct rasta_input_message {
    uint16_t msgid;
    int16_t par1;
    int16_t par2;
};

#endif
