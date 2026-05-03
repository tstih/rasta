// Declares command-line parsing for the rasta SDL2 raster emulator.
// The parser validates raster dimensions, pixel packing, UDP settings,
// and the framebuffer backing-file path used for mmap emulation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 tomaz stih

#ifndef RASTA_OPTIONS_H
#define RASTA_OPTIONS_H

#include <cstdint>
#include <string>

struct rasta_options {
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t bits_per_pixel;
    std::string framebuffer_path;
    std::uint16_t input_port;
    std::uint32_t window_scale;
    bool show_cursor;
};

// Returns the default emulator configuration used at startup and as the
// baseline for subscriber-driven reconfiguration.
rasta_options default_rasta_options();

// Parses command-line arguments into emulator options.
// All settings are optional and fall back to default_rasta_options().
// Optional flags: --width <n>, --height <n>, --bpp <n>,
// --framebuffer <path>, --port <udp_port>, --scale <n>, and
// --cursor <on|off>.
// Returns true on success and false when arguments are invalid.
bool parse_arguments(int argc, char **argv, rasta_options &options,
    std::string &error_message);

// Parses a whitespace-delimited option string using the same syntax as
// the command line. Unspecified fields inherit from base_options.
// Subscriber reconfiguration intentionally rejects --port because the
// UDP socket is already bound when the datagram is received.
bool parse_argument_string(const std::string &text,
    const rasta_options &base_options, rasta_options &options,
    std::string &error_message);

// Returns a short usage string suitable for terminal help output.
std::string build_usage(const char *program_name);

#endif
