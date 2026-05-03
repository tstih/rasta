// Declares a small C client API for talking to the rasta emulator.
// The helpers wrap mmap framebuffer access and UDP input subscription so
// C11 sample code can stay focused on its own behavior.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 tomaz stih

#ifndef RASTA_CLIENT_H
#define RASTA_CLIENT_H

#include "rasta_input_protocol.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct rasta_framebuffer {
    uint32_t width;
    uint32_t height;
    uint32_t bits_per_pixel;
    size_t size_bytes;
    int fd;
    uint8_t *data;
};

struct rasta_input {
    int socket_fd;
};

size_t rasta_framebuffer_size(uint32_t width, uint32_t height,
    uint32_t bits_per_pixel);

int rasta_framebuffer_open(struct rasta_framebuffer *framebuffer,
    uint32_t width, uint32_t height, uint32_t bits_per_pixel,
    const char *path);

void rasta_framebuffer_close(struct rasta_framebuffer *framebuffer);

uint32_t rasta_framebuffer_get_pixel(const struct rasta_framebuffer *framebuffer,
    uint32_t x, uint32_t y);

void rasta_framebuffer_set_pixel(struct rasta_framebuffer *framebuffer,
    uint32_t x, uint32_t y, uint32_t value);

int rasta_framebuffer_sync(struct rasta_framebuffer *framebuffer);

int rasta_input_open(struct rasta_input *input, const char *host,
    uint16_t port);

int rasta_input_open_with_subscription(struct rasta_input *input,
    const char *host, uint16_t port, const char *subscription);

void rasta_input_close(struct rasta_input *input);

int rasta_input_receive(struct rasta_input *input,
    struct rasta_input_message *message);

bool rasta_input_message_has_position(uint16_t msgid);

#endif
