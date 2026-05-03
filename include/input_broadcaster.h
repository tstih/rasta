// Declares UDP input event broadcasting for the rasta SDL2 window.
// Clients subscribe by sending any datagram to the configured port, and
// subsequent keyboard and mouse events are sent back as fixed-size
// binary messages with a message id and two integer parameters.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 tomaz stih

#ifndef INPUT_BROADCASTER_H
#define INPUT_BROADCASTER_H

#include <cstdint>
#include <string>

#include <sys/socket.h>

union SDL_Event;

class input_broadcaster {
public:
    explicit input_broadcaster(std::uint16_t port);
    ~input_broadcaster();

    input_broadcaster(const input_broadcaster &) = delete;
    input_broadcaster &operator=(const input_broadcaster &) = delete;

    // Polls the UDP socket for subscriber registration datagrams.
    // Returns true when at least one datagram was received and stores the
    // most recent payload into latest_payload.
    bool poll_subscribers(std::string &latest_payload);

    // Sends one SDL input event to the most recent subscriber, if any.
    void broadcast_event(const SDL_Event &event);

    // Returns the local UDP port used for subscriber registration.
    std::uint16_t port() const;

    // Returns a human-readable description of the current subscriber.
    std::string subscriber_description() const;

private:
    void broadcast_key_event(const SDL_Event &event);
    void broadcast_mouse_motion_event(const SDL_Event &event);
    void broadcast_mouse_button_event(const SDL_Event &event);
    void send_message(std::uint16_t msgid, std::int16_t par1,
        std::int16_t par2);

    int socket_fd_;
    std::uint16_t port_;
    bool has_subscriber_;
    struct sockaddr_storage *subscriber_storage_;
    socklen_t subscriber_length_;
};

#endif
