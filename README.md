# rasta

`rasta` is an SDL2 raster emulator backed by a memory-mapped framebuffer
file. It continuously decodes a packed linear raster into an SDL window
and broadcasts keyboard and mouse events over UDP datagrams.

## Features

- Packed linear framebuffer stored in an mmap-backed file.
- Sub-byte pixel formats with `1`, `2`, or `3` bits per pixel.
- `3` bits per pixel maps to binary `RGB` with no alpha channel.
- SDL2 window that redraws the mapped framebuffer as fast as possible.
- Runtime zoom with `+` and `-`.
- UDP input event broadcast to the most recent client that subscribes.

## Dependencies

- GCC with C++20 support
- CMake 3.20 or newer
- SDL2 development package
- `pkg-config`

On Ubuntu:

```sh
sudo apt-get install build-essential cmake libsdl2-dev pkg-config
```

## Build

Configure with classic Makefiles:

```sh
cmake -S . -B build -G "Unix Makefiles"
cmake --build build
```

Run tests:

```sh
ctest --test-dir build --output-on-failure
```

## Usage

```sh
./bin/rasta --width <width> --height <height> [--bpp <bits_per_pixel>] \
    [--framebuffer <path>] [--port <udp_port>] [--scale <window_scale>] \
    [--cursor <on|off>]
```

Example:

```sh
./bin/rasta --width 320 --height 200 --framebuffer /tmp/rasta.fb \
    --port 5000 --scale 2 --cursor on
```

To write something visible into the same shared framebuffer from a
second process:

```sh
./bin/demo1 --width 320 --height 200 --framebuffer /tmp/rasta.fb
```

To animate the pattern for a short run:

```sh
./bin/demo1 --width 320 --height 200 --framebuffer /tmp/rasta.fb \
    --frames 60 --delay-ms 80
```

To subscribe to keyboard and mouse events, print them to the console,
and draw a software cursor in the shared framebuffer:

```sh
./bin/demo2 --width 320 --height 200 --framebuffer /tmp/rasta.fb \
    --port 5000
```

This sample is intended to be used with `rasta --cursor off`.

Startup creates or resizes the framebuffer file to:

```text
ceil(width * height * bits_per_pixel / 8)
```

Pixels are packed linearly and read most-significant-bit first.

If `bits_per_pixel` is omitted, it defaults to `1`.

- `1` bpp: monochrome, `0` = black and `1` = white
- `2` bpp: grayscale, `00` = black through `11` = white
- `3` bpp: binary RGB, bits map to `R`, `G`, and `B`
- `--cursor off`: hide the mouse cursor inside the `rasta` window

## UDP Input Subscription

The emulator binds a UDP socket on the configured port. Any client can
subscribe by sending a datagram to that port. The emulator remembers the
most recent sender and transmits one fixed-size binary message per input
event back to that address.

The message format is:

```c
typedef struct msg_s {
    uint16_t msgid;
    int16_t par1;
    int16_t par2;
} msg_t;
```

All fields are sent in network byte order.

Message ids:

- `1`: key down, `par1 = SDL scancode`, `par2 = 0`
- `2`: key up, `par1 = SDL scancode`, `par2 = 0`
- `3`: mouse move, `par1 = x`, `par2 = y`
- `10`: mouse left down, `par1 = x`, `par2 = y`
- `11`: mouse left up, `par1 = x`, `par2 = y`
- `12`: mouse left click, `par1 = x`, `par2 = y`
- `13`: mouse left double click, `par1 = x`, `par2 = y`
- `20`: mouse middle down, `par1 = x`, `par2 = y`
- `21`: mouse middle up, `par1 = x`, `par2 = y`
- `22`: mouse middle click, `par1 = x`, `par2 = y`
- `23`: mouse middle double click, `par1 = x`, `par2 = y`
- `30`: mouse right down, `par1 = x`, `par2 = y`
- `31`: mouse right up, `par1 = x`, `par2 = y`
- `32`: mouse right click, `par1 = x`, `par2 = y`
- `33`: mouse right double click, `par1 = x`, `par2 = y`
- `40`: mouse x1 down, `par1 = x`, `par2 = y`
- `41`: mouse x1 up, `par1 = x`, `par2 = y`
- `42`: mouse x1 click, `par1 = x`, `par2 = y`
- `43`: mouse x1 double click, `par1 = x`, `par2 = y`
- `50`: mouse x2 down, `par1 = x`, `par2 = y`
- `51`: mouse x2 up, `par1 = x`, `par2 = y`
- `52`: mouse x2 click, `par1 = x`, `par2 = y`
- `53`: mouse x2 double click, `par1 = x`, `par2 = y`

## C11 Framebuffer Example

This example opens the same framebuffer file that `rasta` is using,
maps it into memory, and writes a checkerboard into a `1` bpp raster.

```c
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static size_t framebuffer_size(uint32_t width, uint32_t height,
    uint32_t bits_per_pixel)
{
    size_t bits = (size_t) width * (size_t) height * (size_t) bits_per_pixel;
    return (bits + 7u) / 8u;
}

static void set_pixel(uint8_t *raster, size_t raster_size, uint32_t width,
    uint32_t bits_per_pixel, uint32_t x, uint32_t y, uint32_t value)
{
    size_t pixel_index = (size_t) y * (size_t) width + (size_t) x;
    size_t start_bit = pixel_index * (size_t) bits_per_pixel;
    uint32_t bit;

    for (bit = 0; bit < bits_per_pixel; ++bit) {
        size_t absolute_bit = start_bit + (size_t) bit;
        size_t byte_index = absolute_bit / 8u;
        size_t bit_index = 7u - (absolute_bit % 8u);
        uint8_t mask = (uint8_t) (1u << bit_index);
        uint32_t source_bit =
            (value >> (bits_per_pixel - bit - 1u)) & 1u;

        if (byte_index >= raster_size) {
            return;
        }

        if (source_bit != 0u) {
            raster[byte_index] |= mask;
        } else {
            raster[byte_index] &= (uint8_t) ~mask;
        }
    }
}

int main(void)
{
    const char *path = "/tmp/rasta.fb";
    const uint32_t width = 320;
    const uint32_t height = 200;
    const uint32_t bits_per_pixel = 1;
    const size_t size = framebuffer_size(width, height, bits_per_pixel);
    int fd;
    uint8_t *raster;
    uint32_t x;
    uint32_t y;

    fd = open(path, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open failed: %s\n", strerror(errno));
        return 1;
    }

    raster = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (raster == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    memset(raster, 0, size);

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            uint32_t value = ((x / 8u) + (y / 8u)) & 1u;
            set_pixel(raster, size, width, bits_per_pixel, x, y, value);
        }
    }

    msync(raster, size, MS_SYNC);
    munmap(raster, size);
    close(fd);
    return 0;
}
```

Compile it like this:

```sh
gcc -std=c11 -Wall -Wextra -pedantic -o write_raster write_raster.c
```

Run `rasta` first, then run the writer against the same framebuffer
path.

## C11 Input Example

This example subscribes to the UDP input stream and reads the binary
messages directly with no string parsing.

```c
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct msg_s {
    uint16_t msgid;
    int16_t par1;
    int16_t par2;
} msg_t;

enum {
    MSG_KEY_DOWN = 1,
    MSG_KEY_UP = 2,
    MSG_MOUSE_MOVE = 3,
    MSG_MOUSE_LEFT_DOWN = 10,
    MSG_MOUSE_LEFT_UP = 11,
    MSG_MOUSE_LEFT_CLICK = 12,
    MSG_MOUSE_LEFT_DOUBLE_CLICK = 13
};

int main(void)
{
    const char *host = "127.0.0.1";
    const int port = 5000;
    int fd;
    struct sockaddr_in addr;
    msg_t msg;
    ssize_t received;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        fprintf(stderr, "socket failed: %s\n", strerror(errno));
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short) port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        fprintf(stderr, "inet_pton failed\n");
        close(fd);
        return 1;
    }

    if (sendto(fd, "subscribe", 9, 0, (struct sockaddr *) &addr,
        sizeof(addr)) < 0) {
        fprintf(stderr, "sendto failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    for (;;) {
        received = recvfrom(fd, &msg, sizeof(msg), 0, NULL, NULL);
        if (received < 0) {
            fprintf(stderr, "recvfrom failed: %s\n", strerror(errno));
            close(fd);
            return 1;
        }

        if (received != (ssize_t) sizeof(msg)) {
            continue;
        }

        msg.msgid = ntohs(msg.msgid);
        msg.par1 = (int16_t) ntohs((uint16_t) msg.par1);
        msg.par2 = (int16_t) ntohs((uint16_t) msg.par2);

        switch (msg.msgid) {
        case MSG_KEY_DOWN:
            printf("key down scancode=%d\n", msg.par1);
            break;
        case MSG_KEY_UP:
            printf("key up scancode=%d\n", msg.par1);
            break;
        case MSG_MOUSE_MOVE:
            printf("mouse move x=%d y=%d\n", msg.par1, msg.par2);
            break;
        case MSG_MOUSE_LEFT_DOWN:
            printf("mouse left down x=%d y=%d\n", msg.par1, msg.par2);
            break;
        case MSG_MOUSE_LEFT_UP:
            printf("mouse left up x=%d y=%d\n", msg.par1, msg.par2);
            break;
        case MSG_MOUSE_LEFT_CLICK:
            printf("mouse left click x=%d y=%d\n", msg.par1, msg.par2);
            break;
        case MSG_MOUSE_LEFT_DOUBLE_CLICK:
            printf("mouse left double click x=%d y=%d\n",
                msg.par1, msg.par2);
            break;
        default:
            printf("msgid=%u par1=%d par2=%d\n",
                msg.msgid, msg.par1, msg.par2);
            break;
        }

        fflush(stdout);
    }
}
```

Compile it like this:

```sh
gcc -std=c11 -Wall -Wextra -pedantic -o read_input read_input.c
```

Start `read_input`, then click and type inside the `rasta` window.

## Notes

- The framebuffer is tightly packed with no per-row padding.
- The current implementation is single-window and single-subscriber.
- The renderer uses nearest-neighbor scaling for crisp low-bpp output.
- Press `+` or `-` in the SDL window to zoom in or out while running.
- `demo1` can be started after `rasta` to prove the shared-memory
  display path is working.
- `demo2` is a plain C11 sample that prints input events and draws a
  software cursor into the framebuffer.
