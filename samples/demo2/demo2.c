// Subscribes to rasta input events and draws a software mouse cursor.
// This C11 sample prints keyboard and mouse events to the console while
// updating a cursor directly in the mmap-backed framebuffer.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 tomaz stih

#include "rasta_client.h"
#include "rasta_input_protocol.h"

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum {
    cursor_max_points = 128
};

struct options {
    uint32_t width;
    uint32_t height;
    uint32_t bits_per_pixel;
    const char *framebuffer_path;
    const char *host;
    uint16_t port;
    bool inverse_pixels;
};

struct cursor_state {
    bool visible;
    int16_t x;
    int16_t y;
    uint32_t saved_values[cursor_max_points];
};

struct cursor_point {
    int8_t dx;
    int8_t dy;
    uint8_t color;
};

static const struct cursor_point cursor_points[] = {
    {0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {0, 2, 0}, {1, 2, 1}, {2, 2, 0},
    {0, 3, 0}, {1, 3, 1}, {2, 3, 1}, {3, 3, 0}, {0, 4, 0}, {1, 4, 1},
    {2, 4, 1}, {3, 4, 1}, {4, 4, 0}, {0, 5, 0}, {1, 5, 1}, {2, 5, 1},
    {3, 5, 1}, {4, 5, 1}, {5, 5, 0}, {0, 6, 0}, {1, 6, 1}, {2, 6, 1},
    {3, 6, 1}, {4, 6, 1}, {5, 6, 1}, {6, 6, 0}, {0, 7, 0}, {1, 7, 1},
    {2, 7, 1}, {3, 7, 1}, {4, 7, 1}, {5, 7, 1}, {6, 7, 1}, {7, 7, 0},
    {0, 8, 0}, {1, 8, 1}, {2, 8, 1}, {3, 8, 1}, {4, 8, 1}, {5, 8, 1},
    {6, 8, 1}, {7, 8, 1}, {8, 8, 0}, {0, 9, 0}, {1, 9, 1}, {2, 9, 1},
    {3, 9, 1}, {4, 9, 1}, {5, 9, 1}, {6, 9, 1},
    {7, 9, 1}, {8, 9, 1}, {9, 9, 0}, {0, 10, 0}, {1, 10, 1}, {2, 10, 1},
    {3, 10, 1}, {4, 10, 1}, {5, 10, 1}, {6, 10, 1}, {7, 10, 1},
    {8, 10, 1}, {9, 10, 1}, {10, 10, 0}, {0, 11, 0}, {1, 11, 1},
    {2, 11, 1}, {3, 11, 1}, {4, 11, 0}, {5, 11, 0}, {6, 11, 0},
    {7, 11, 0}, {8, 11, 0}, {9, 11, 0}, {10, 11, 0}, {0, 12, 0},
    {1, 12, 1}, {2, 12, 1}, {3, 12, 0}, {4, 12, 1}, {5, 12, 1},
    {6, 12, 0}, {0, 13, 0}, {1, 13, 1}, {2, 13, 0}, {4, 13, 0},
    {5, 13, 1}, {6, 13, 1}, {7, 13, 0}, {0, 14, 0}, {1, 14, 0},
    {4, 14, 0}, {5, 14, 1}, {6, 14, 1}, {7, 14, 0}, {0, 15, 0},
    {5, 15, 0}, {6, 15, 1}, {7, 15, 1}, {8, 15, 0}, {6, 16, 0},
    {7, 16, 1}, {8, 16, 1}, {9, 16, 0}, {7, 17, 0}, {8, 17, 0}
};

enum {
    cursor_point_count = (int) (sizeof(cursor_points) / sizeof(cursor_points[0]))
};

static volatile sig_atomic_t stop_requested = 0;

static void on_signal(int signum)
{
    (void) signum;
    stop_requested = 1;
}

static uint32_t max_pixel_value(uint32_t bits_per_pixel)
{
    if (bits_per_pixel == 32u) {
        return UINT32_MAX;
    }
    return (1u << bits_per_pixel) - 1u;
}

static bool parse_u32(const char *text, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed = strtoul(text, &end, 10);
    if (end == text || *end != '\0') {
        return false;
    }
    *value = (uint32_t) parsed;
    return true;
}

static bool parse_u16(const char *text, uint16_t *value)
{
    uint32_t parsed = 0;
    if (!parse_u32(text, &parsed) || parsed > 65535u) {
        return false;
    }
    *value = (uint16_t) parsed;
    return true;
}

static void print_usage(const char *program_name)
{
    printf("Usage: %s [--width <width>] [--height <height>] "
           "[--bpp <bits_per_pixel>] [--framebuffer <path>] "
           "[--host <host>] [--port <port>] [--inverse <on|off>]\n",
        program_name);
}

static bool parse_arguments(int argc, char **argv, struct options *options)
{
    int index;

    options->width = 320;
    options->height = 200;
    options->bits_per_pixel = 1;
    options->framebuffer_path = "/tmp/rasta.framebuffer";
    options->host = "127.0.0.1";
    options->port = 5000;
    options->inverse_pixels = false;

    for (index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--width") == 0) {
            if (index + 1 >= argc || !parse_u32(argv[++index], &options->width)
                || options->width == 0u) {
                fprintf(stderr, "error: invalid --width\n");
                return false;
            }
        } else if (strcmp(argv[index], "--height") == 0) {
            if (index + 1 >= argc || !parse_u32(argv[++index], &options->height)
                || options->height == 0u) {
                fprintf(stderr, "error: invalid --height\n");
                return false;
            }
        } else if (strcmp(argv[index], "--bpp") == 0) {
            if (index + 1 >= argc ||
                !parse_u32(argv[++index], &options->bits_per_pixel) ||
                (options->bits_per_pixel != 1u &&
                 options->bits_per_pixel != 2u &&
                 options->bits_per_pixel != 3u &&
                 options->bits_per_pixel != 32u)) {
                fprintf(stderr, "error: invalid --bpp\n");
                return false;
            }
        } else if (strcmp(argv[index], "--framebuffer") == 0) {
            if (index + 1 >= argc) {
                fprintf(stderr, "error: missing --framebuffer value\n");
                return false;
            }
            options->framebuffer_path = argv[++index];
        } else if (strcmp(argv[index], "--host") == 0) {
            if (index + 1 >= argc) {
                fprintf(stderr, "error: missing --host value\n");
                return false;
            }
            options->host = argv[++index];
        } else if (strcmp(argv[index], "--port") == 0) {
            if (index + 1 >= argc || !parse_u16(argv[++index], &options->port)
                || options->port == 0u) {
                fprintf(stderr, "error: invalid --port\n");
                return false;
            }
        } else if (strcmp(argv[index], "--inverse") == 0) {
            if (index + 1 >= argc) {
                fprintf(stderr, "error: missing --inverse value\n");
                return false;
            }
            ++index;
            if (strcmp(argv[index], "on") == 0) {
                options->inverse_pixels = true;
            } else if (strcmp(argv[index], "off") == 0) {
                options->inverse_pixels = false;
            } else {
                fprintf(stderr, "error: invalid --inverse\n");
                return false;
            }
        } else if (strcmp(argv[index], "--help") == 0 ||
                   strcmp(argv[index], "-h") == 0) {
            return false;
        } else {
            fprintf(stderr, "error: unknown option: %s\n", argv[index]);
            return false;
        }
    }

    return true;
}

static int build_subscription(char *buffer, size_t size,
    const struct options *options)
{
    if (buffer == NULL || size == 0u || options == NULL) {
        errno = EINVAL;
        return -1;
    }

    return snprintf(buffer, size,
        "--width %u --height %u --bpp %u --framebuffer %s --inverse %s",
        (unsigned) options->width, (unsigned) options->height,
        (unsigned) options->bits_per_pixel, options->framebuffer_path,
        options->inverse_pixels ? "on" : "off");
}

static void erase_cursor(struct rasta_framebuffer *framebuffer,
    struct cursor_state *cursor)
{
    uint32_t index;

    if (!cursor->visible) {
        return;
    }

    for (index = 0; index < cursor_point_count; ++index) {
        int x = cursor->x + cursor_points[index].dx;
        int y = cursor->y + cursor_points[index].dy;
        if (x < 0 || y < 0 || x >= (int) framebuffer->width ||
            y >= (int) framebuffer->height) {
            continue;
        }

        rasta_framebuffer_set_pixel(framebuffer, (uint32_t) x, (uint32_t) y,
            cursor->saved_values[index]);
    }

    cursor->visible = false;
}

static void draw_cursor(struct rasta_framebuffer *framebuffer,
    struct cursor_state *cursor,
    int16_t x, int16_t y)
{
    uint32_t index;
    uint32_t max_value = max_pixel_value(framebuffer->bits_per_pixel);

    cursor->x = x;
    cursor->y = y;

    for (index = 0; index < cursor_point_count; ++index) {
        int px = x + cursor_points[index].dx;
        int py = y + cursor_points[index].dy;
        uint32_t cursor_value;
        if (px < 0 || py < 0 || px >= (int) framebuffer->width ||
            py >= (int) framebuffer->height) {
            cursor->saved_values[index] = 0;
            continue;
        }

        cursor->saved_values[index] = rasta_framebuffer_get_pixel(framebuffer,
            (uint32_t) px, (uint32_t) py);
        cursor_value = cursor_points[index].color != 0 ? max_value : 0u;
        rasta_framebuffer_set_pixel(framebuffer, (uint32_t) px, (uint32_t) py,
            cursor_value);
    }

    cursor->visible = true;
}

static const char *message_name(uint16_t msgid)
{
    switch (msgid) {
    case rasta_msg_key_down:
        return "key_down";
    case rasta_msg_key_up:
        return "key_up";
    case rasta_msg_mouse_move:
        return "mouse_move";
    case rasta_msg_mouse_left_down:
        return "mouse_left_down";
    case rasta_msg_mouse_left_up:
        return "mouse_left_up";
    case rasta_msg_mouse_left_click:
        return "mouse_left_click";
    case rasta_msg_mouse_left_double_click:
        return "mouse_left_double_click";
    case rasta_msg_mouse_middle_down:
        return "mouse_middle_down";
    case rasta_msg_mouse_middle_up:
        return "mouse_middle_up";
    case rasta_msg_mouse_middle_click:
        return "mouse_middle_click";
    case rasta_msg_mouse_middle_double_click:
        return "mouse_middle_double_click";
    case rasta_msg_mouse_right_down:
        return "mouse_right_down";
    case rasta_msg_mouse_right_up:
        return "mouse_right_up";
    case rasta_msg_mouse_right_click:
        return "mouse_right_click";
    case rasta_msg_mouse_right_double_click:
        return "mouse_right_double_click";
    case rasta_msg_mouse_x1_down:
        return "mouse_x1_down";
    case rasta_msg_mouse_x1_up:
        return "mouse_x1_up";
    case rasta_msg_mouse_x1_click:
        return "mouse_x1_click";
    case rasta_msg_mouse_x1_double_click:
        return "mouse_x1_double_click";
    case rasta_msg_mouse_x2_down:
        return "mouse_x2_down";
    case rasta_msg_mouse_x2_up:
        return "mouse_x2_up";
    case rasta_msg_mouse_x2_click:
        return "mouse_x2_click";
    case rasta_msg_mouse_x2_double_click:
        return "mouse_x2_double_click";
    default:
        return "unknown";
    }
}

static void print_message(const struct rasta_input_message *message)
{
    uint16_t msgid = message->msgid;
    int16_t par1 = message->par1;
    int16_t par2 = message->par2;

    if (msgid == rasta_msg_key_down || msgid == rasta_msg_key_up) {
        printf("%s scancode=%d\n", message_name(msgid), par1);
    } else {
        printf("%s x=%d y=%d\n", message_name(msgid), par1, par2);
    }
    fflush(stdout);
}

int main(int argc, char **argv)
{
    struct options options;
    struct cursor_state cursor;
    struct rasta_framebuffer framebuffer;
    struct rasta_input input;
    struct rasta_input_message message;
    char subscription[512];
    int subscription_length;

    if (!parse_arguments(argc, argv, &options)) {
        print_usage(argv[0]);
        return 1;
    }

    memset(&cursor, 0, sizeof(cursor));
    memset(&framebuffer, 0, sizeof(framebuffer));
    framebuffer.fd = -1;
    memset(&input, 0, sizeof(input));
    input.socket_fd = -1;
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    subscription_length = build_subscription(subscription,
        sizeof(subscription), &options);
    if (subscription_length < 0 ||
        (size_t) subscription_length >= sizeof(subscription)) {
        fprintf(stderr, "unable to build subscription datagram\n");
        return 1;
    }

    if (rasta_input_open_with_subscription(&input, options.host, options.port,
        subscription) != 0) {
        fprintf(stderr, "rasta_input_open_with_subscription failed: %s\n",
            strerror(errno));
        return 1;
    }

    if (rasta_framebuffer_open(&framebuffer, options.width, options.height,
        options.bits_per_pixel, options.framebuffer_path) != 0) {
        fprintf(stderr, "rasta_framebuffer_open failed: %s\n",
            strerror(errno));
        rasta_input_close(&input);
        return 1;
    }

    printf("demo2 subscribed to %s:%u with '%s'\n", options.host,
        (unsigned) options.port, subscription);
    fflush(stdout);

    while (!stop_requested) {
        if (rasta_input_receive(&input, &message) != 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EPROTO) {
                continue;
            }
            fprintf(stderr, "rasta_input_receive failed: %s\n",
                strerror(errno));
            break;
        }

        print_message(&message);

        if (rasta_input_message_has_position(message.msgid)) {
            erase_cursor(&framebuffer, &cursor);
            draw_cursor(&framebuffer, &cursor, message.par1, message.par2);
            rasta_framebuffer_sync(&framebuffer);
        }
    }

    erase_cursor(&framebuffer, &cursor);
    rasta_framebuffer_sync(&framebuffer);
    rasta_input_close(&input);
    rasta_framebuffer_close(&framebuffer);
    return 0;
}
