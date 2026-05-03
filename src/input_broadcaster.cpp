// Implements UDP broadcasting of SDL keyboard and mouse input events.
// The broadcaster tracks the most recent datagram sender as its active
// subscriber and streams one fixed-size binary message per UDP datagram.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 tomaz stih

#include "input_broadcaster.h"

#include "rasta_input_protocol.h"

#include <SDL.h>

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <stdexcept>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

using subscriber_storage = sockaddr_storage;

std::runtime_error make_errno_error(const std::string &message)
{
    return std::runtime_error(message + ": " + std::strerror(errno));
}

std::uint16_t encode_i16(std::int16_t value)
{
    return htons(static_cast<std::uint16_t>(value));
}

std::uint16_t mouse_button_base_id(std::uint8_t button)
{
    switch (button) {
    case SDL_BUTTON_LEFT:
        return rasta_msg_mouse_left_down;
    case SDL_BUTTON_MIDDLE:
        return rasta_msg_mouse_middle_down;
    case SDL_BUTTON_RIGHT:
        return rasta_msg_mouse_right_down;
    case SDL_BUTTON_X1:
        return rasta_msg_mouse_x1_down;
    case SDL_BUTTON_X2:
        return rasta_msg_mouse_x2_down;
    default:
        return 0;
    }
}

} // namespace

input_broadcaster::input_broadcaster(std::uint16_t port)
    : socket_fd_(-1),
      port_(port),
      has_subscriber_(false),
      subscriber_storage_(new sockaddr_storage {}),
      subscriber_length_(0)
{
    socket_fd_ = socket(AF_INET6, SOCK_DGRAM, 0);
    if (socket_fd_ < 0) {
        delete subscriber_storage_;
        subscriber_storage_ = nullptr;
        throw make_errno_error("unable to create UDP socket");
    }

    constexpr int reuse = 1;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse,
        sizeof(reuse)) != 0) {
        throw make_errno_error("unable to configure UDP socket");
    }

    int v6_only = 0;
    if (setsockopt(socket_fd_, IPPROTO_IPV6, IPV6_V6ONLY, &v6_only,
        sizeof(v6_only)) != 0) {
        throw make_errno_error("unable to configure dual-stack UDP socket");
    }

    const int flags = fcntl(socket_fd_, F_GETFL, 0);
    if (flags < 0 || fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK) != 0) {
        throw make_errno_error("unable to configure non-blocking UDP socket");
    }

    sockaddr_in6 address {};
    address.sin6_family = AF_INET6;
    address.sin6_addr = in6addr_any;
    address.sin6_port = htons(port_);

    if (bind(socket_fd_, reinterpret_cast<sockaddr *>(&address),
        sizeof(address)) != 0) {
        throw make_errno_error("unable to bind UDP socket");
    }
}

input_broadcaster::~input_broadcaster()
{
    if (socket_fd_ >= 0) {
        close(socket_fd_);
    }
    delete subscriber_storage_;
}

bool input_broadcaster::poll_subscribers(std::string &latest_payload)
{
    if (subscriber_storage_ == nullptr) {
        return false;
    }

    latest_payload.clear();
    std::array<char, 512> probe {};
    bool received_any = false;
    for (;;) {
        socklen_t sender_length = sizeof(*subscriber_storage_);
        const ssize_t received = recvfrom(socket_fd_, probe.data(),
            probe.size(), 0,
            reinterpret_cast<sockaddr *>(subscriber_storage_), &sender_length);

        if (received >= 0) {
            has_subscriber_ = true;
            subscriber_length_ = sender_length;
            latest_payload.assign(probe.data(),
                static_cast<std::size_t>(received));
            received_any = true;
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return received_any;
        }

        throw make_errno_error("unable to receive subscriber datagram");
    }
}

void input_broadcaster::broadcast_event(const SDL_Event &event)
{
    switch (event.type) {
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        broadcast_key_event(event);
        break;
    case SDL_MOUSEMOTION:
        broadcast_mouse_motion_event(event);
        break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        broadcast_mouse_button_event(event);
        break;
    default:
        break;
    }
}

std::uint16_t input_broadcaster::port() const
{
    return port_;
}

std::string input_broadcaster::subscriber_description() const
{
    if (!has_subscriber_ || subscriber_storage_ == nullptr) {
        return "none";
    }

    char host[INET6_ADDRSTRLEN] = {0};
    std::uint16_t port = 0;

    if (subscriber_storage_->ss_family == AF_INET) {
        const sockaddr_in *address =
            reinterpret_cast<const sockaddr_in *>(subscriber_storage_);
        inet_ntop(AF_INET, &address->sin_addr, host, sizeof(host));
        port = ntohs(address->sin_port);
    } else if (subscriber_storage_->ss_family == AF_INET6) {
        const sockaddr_in6 *address =
            reinterpret_cast<const sockaddr_in6 *>(subscriber_storage_);
        inet_ntop(AF_INET6, &address->sin6_addr, host, sizeof(host));
        port = ntohs(address->sin6_port);
    }

    return std::string(host) + ":" + std::to_string(port);
}

void input_broadcaster::broadcast_key_event(const SDL_Event &event)
{
    const std::uint16_t msgid =
        event.key.state == SDL_PRESSED ? rasta_msg_key_down : rasta_msg_key_up;
    send_message(msgid, static_cast<std::int16_t>(event.key.keysym.scancode), 0);
}

void input_broadcaster::broadcast_mouse_motion_event(const SDL_Event &event)
{
    send_message(rasta_msg_mouse_move, event.motion.x, event.motion.y);
}

void input_broadcaster::broadcast_mouse_button_event(const SDL_Event &event)
{
    const std::uint16_t base_msgid = mouse_button_base_id(event.button.button);
    if (base_msgid == 0U) {
        return;
    }

    const std::uint16_t state_msgid = event.button.state == SDL_PRESSED ?
        base_msgid : (base_msgid + 1U);
    send_message(state_msgid, static_cast<std::int16_t>(event.button.x),
        static_cast<std::int16_t>(event.button.y));

    if (event.type == SDL_MOUSEBUTTONUP) {
        if (event.button.clicks == 1U) {
            send_message(base_msgid + 2U,
                static_cast<std::int16_t>(event.button.x),
                static_cast<std::int16_t>(event.button.y));
        } else if (event.button.clicks >= 2U) {
            send_message(base_msgid + 3U,
                static_cast<std::int16_t>(event.button.x),
                static_cast<std::int16_t>(event.button.y));
        }
    }
}

void input_broadcaster::send_message(std::uint16_t msgid, std::int16_t par1,
    std::int16_t par2)
{
    if (!has_subscriber_ || subscriber_storage_ == nullptr) {
        return;
    }

    const rasta_input_message message {
        .msgid = htons(msgid),
        .par1 = static_cast<std::int16_t>(encode_i16(par1)),
        .par2 = static_cast<std::int16_t>(encode_i16(par2)),
    };

    const ssize_t sent = sendto(socket_fd_, &message, sizeof(message), 0,
        reinterpret_cast<const sockaddr *>(subscriber_storage_),
        subscriber_length_);

    if (sent < 0 && errno != ECONNREFUSED && errno != EHOSTUNREACH &&
        errno != ENETUNREACH) {
        throw make_errno_error("unable to send input datagram");
    }
}
