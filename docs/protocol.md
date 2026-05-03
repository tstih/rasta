# Input Protocol

The emulator accepts UDP subscription datagrams on the configured input
port. The most recent sender becomes the active subscriber for future
keyboard and mouse event packets.

If a subscription datagram begins with `-`, the payload is parsed with
the same option syntax as the command line before the subscriber becomes
active. This allows the client to reconfigure width, height, bpp,
framebuffer path, scale, and cursor visibility at runtime. Omitted
fields keep their current values.

Subscriber datagrams cannot change `--port`, because the socket is
already bound when the reconfiguration request is received.

Each event is sent as one fixed-size binary message:

```c
typedef struct msg_s {
    uint16_t msgid;
    int16_t par1;
    int16_t par2;
} msg_t;
```

All fields are sent in network byte order.

Message ids:

- `1`: key down
- `2`: key up
- `3`: mouse move
- `10..13`: left button down, up, click, double click
- `20..23`: middle button down, up, click, double click
- `30..33`: right button down, up, click, double click
- `40..43`: x1 button down, up, click, double click
- `50..53`: x2 button down, up, click, double click

Parameters:

- For key events: `par1 = SDL scancode`, `par2 = 0`
- For mouse events: `par1 = x`, `par2 = y`
