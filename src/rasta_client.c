// Implements a small C client API for the rasta emulator.
// The helpers hide mmap and UDP setup so client code can read events
// and write pixels with straightforward C11 calls.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 tomaz stih

#include "rasta_client.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

size_t rasta_framebuffer_size(uint32_t width, uint32_t height,
    uint32_t bits_per_pixel)
{
    uint64_t bits = (uint64_t) width * (uint64_t) height *
        (uint64_t) bits_per_pixel;
    return (size_t) ((bits + 7u) / 8u);
}

int rasta_framebuffer_open(struct rasta_framebuffer *framebuffer,
    uint32_t width, uint32_t height, uint32_t bits_per_pixel,
    const char *path)
{
    if (framebuffer == NULL || path == NULL ||
        (bits_per_pixel != 1u && bits_per_pixel != 2u &&
         bits_per_pixel != 3u && bits_per_pixel != 32u)) {
        errno = EINVAL;
        return -1;
    }

    framebuffer->width = width;
    framebuffer->height = height;
    framebuffer->bits_per_pixel = bits_per_pixel;
    framebuffer->size_bytes = rasta_framebuffer_size(width, height,
        bits_per_pixel);
    framebuffer->fd = open(path, O_RDWR);
    framebuffer->data = NULL;

    if (framebuffer->fd < 0) {
        return -1;
    }

    framebuffer->data = mmap(NULL, framebuffer->size_bytes,
        PROT_READ | PROT_WRITE, MAP_SHARED, framebuffer->fd, 0);
    if (framebuffer->data == MAP_FAILED) {
        framebuffer->data = NULL;
        close(framebuffer->fd);
        framebuffer->fd = -1;
        return -1;
    }

    return 0;
}

void rasta_framebuffer_close(struct rasta_framebuffer *framebuffer)
{
    if (framebuffer == NULL) {
        return;
    }

    if (framebuffer->data != NULL) {
        munmap(framebuffer->data, framebuffer->size_bytes);
        framebuffer->data = NULL;
    }
    if (framebuffer->fd >= 0) {
        close(framebuffer->fd);
        framebuffer->fd = -1;
    }
}

uint32_t rasta_framebuffer_get_pixel(const struct rasta_framebuffer *framebuffer,
    uint32_t x, uint32_t y)
{
    size_t pixel_index;
    size_t start_bit;
    uint32_t value = 0;
    uint32_t bit;

    if (framebuffer == NULL || framebuffer->data == NULL ||
        x >= framebuffer->width || y >= framebuffer->height) {
        return 0;
    }

    pixel_index = (size_t) y * (size_t) framebuffer->width + (size_t) x;
    start_bit = pixel_index * (size_t) framebuffer->bits_per_pixel;

    for (bit = 0; bit < framebuffer->bits_per_pixel; ++bit) {
        size_t absolute_bit = start_bit + (size_t) bit;
        size_t byte_index = absolute_bit / 8u;
        size_t bit_index = 7u - (absolute_bit % 8u);
        if (byte_index >= framebuffer->size_bytes) {
            break;
        }

        value <<= 1u;
        value |= (uint32_t)
            ((framebuffer->data[byte_index] >> bit_index) & 1u);
    }

    return value;
}

void rasta_framebuffer_set_pixel(struct rasta_framebuffer *framebuffer,
    uint32_t x, uint32_t y, uint32_t value)
{
    size_t pixel_index;
    size_t start_bit;
    uint32_t bit;

    if (framebuffer == NULL || framebuffer->data == NULL ||
        x >= framebuffer->width || y >= framebuffer->height) {
        return;
    }

    pixel_index = (size_t) y * (size_t) framebuffer->width + (size_t) x;
    start_bit = pixel_index * (size_t) framebuffer->bits_per_pixel;

    for (bit = 0; bit < framebuffer->bits_per_pixel; ++bit) {
        size_t absolute_bit = start_bit + (size_t) bit;
        size_t byte_index = absolute_bit / 8u;
        size_t bit_index = 7u - (absolute_bit % 8u);
        uint8_t mask;
        uint32_t source_bit;

        if (byte_index >= framebuffer->size_bytes) {
            return;
        }

        mask = (uint8_t) (1u << bit_index);
        source_bit =
            (value >> (framebuffer->bits_per_pixel - bit - 1u)) & 1u;
        if (source_bit != 0u) {
            framebuffer->data[byte_index] |= mask;
        } else {
            framebuffer->data[byte_index] &= (uint8_t) ~mask;
        }
    }
}

int rasta_framebuffer_sync(struct rasta_framebuffer *framebuffer)
{
    if (framebuffer == NULL || framebuffer->data == NULL) {
        errno = EINVAL;
        return -1;
    }

    return msync(framebuffer->data, framebuffer->size_bytes, MS_ASYNC);
}

int rasta_input_open_with_subscription(struct rasta_input *input,
    const char *host, uint16_t port, const char *subscription)
{
    struct sockaddr_in addr;
    const char *payload;
    size_t payload_length;

    if (input == NULL || host == NULL || port == 0u) {
        errno = EINVAL;
        return -1;
    }

    payload = subscription != NULL ? subscription : "subscribe";
    payload_length = strlen(payload);

    input->socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (input->socket_fd < 0) {
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        close(input->socket_fd);
        input->socket_fd = -1;
        errno = EINVAL;
        return -1;
    }

    if (sendto(input->socket_fd, payload, payload_length, 0,
        (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        close(input->socket_fd);
        input->socket_fd = -1;
        return -1;
    }

    return 0;
}

int rasta_input_open(struct rasta_input *input, const char *host, uint16_t port)
{
    return rasta_input_open_with_subscription(input, host, port, "subscribe");
}

void rasta_input_close(struct rasta_input *input)
{
    if (input == NULL) {
        return;
    }

    if (input->socket_fd >= 0) {
        close(input->socket_fd);
        input->socket_fd = -1;
    }
}

int rasta_input_receive(struct rasta_input *input,
    struct rasta_input_message *message)
{
    ssize_t received;

    if (input == NULL || message == NULL || input->socket_fd < 0) {
        errno = EINVAL;
        return -1;
    }

    received = recvfrom(input->socket_fd, message, sizeof(*message), 0,
        NULL, NULL);
    if (received < 0) {
        return -1;
    }
    if (received != (ssize_t) sizeof(*message)) {
        errno = EPROTO;
        return -1;
    }

    message->msgid = ntohs(message->msgid);
    message->par1 = (int16_t) ntohs((uint16_t) message->par1);
    message->par2 = (int16_t) ntohs((uint16_t) message->par2);
    return 0;
}

bool rasta_input_message_has_position(uint16_t msgid)
{
    return msgid == rasta_msg_mouse_move ||
        (msgid >= rasta_msg_mouse_left_down &&
         msgid <= rasta_msg_mouse_x2_double_click);
}
