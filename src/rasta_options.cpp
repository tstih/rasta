// Implements command-line parsing for the rasta SDL2 raster emulator.
// The parser keeps setup minimal while validating packed-pixel formats,
// framebuffer file settings, and UDP event streaming parameters.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 tomaz stih

#include "rasta_options.h"

#include <cstdlib>
#include <limits>
#include <sstream>
#include <string>

namespace {

bool parse_u32(const char *text, std::uint32_t &value)
{
    char *end = nullptr;
    const unsigned long parsed = std::strtoul(text, &end, 10);
    if (end == text || *end != '\0') {
        return false;
    }
    if (parsed > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    value = static_cast<std::uint32_t>(parsed);
    return true;
}

bool parse_u16(const char *text, std::uint16_t &value)
{
    std::uint32_t parsed = 0;
    if (!parse_u32(text, parsed)) {
        return false;
    }
    if (parsed > std::numeric_limits<std::uint16_t>::max()) {
        return false;
    }
    value = static_cast<std::uint16_t>(parsed);
    return true;
}

bool parse_cursor_mode(const std::string &text, bool &show_cursor)
{
    if (text == "on") {
        show_cursor = true;
        return true;
    }
    if (text == "off") {
        show_cursor = false;
        return true;
    }
    return false;
}

} // namespace

bool parse_arguments(int argc, char **argv, rasta_options &options,
    std::string &error_message)
{
    if (argc >= 2) {
        const std::string first_argument = argv[1];
        if (first_argument == "--help" || first_argument == "-h") {
            error_message.clear();
            return false;
        }
    }

    options = {
        .width = 0,
        .height = 0,
        .bits_per_pixel = 1,
        .framebuffer_path = "/tmp/rasta.framebuffer",
        .input_port = 5000,
        .window_scale = 4,
        .show_cursor = true,
    };

    bool has_width = false;
    bool has_height = false;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--width") {
            if (index + 1 >= argc) {
                error_message = "missing value for --width";
                return false;
            }
            if (!parse_u32(argv[++index], options.width) || options.width == 0) {
                error_message = "width must be a positive integer";
                return false;
            }
            has_width = true;
        } else if (argument == "--height") {
            if (index + 1 >= argc) {
                error_message = "missing value for --height";
                return false;
            }
            if (!parse_u32(argv[++index], options.height) ||
                options.height == 0) {
                error_message = "height must be a positive integer";
                return false;
            }
            has_height = true;
        } else if (argument == "--bpp") {
            if (index + 1 >= argc) {
                error_message = "missing value for --bpp";
                return false;
            }
            if (!parse_u32(argv[++index], options.bits_per_pixel)) {
                error_message = "bits_per_pixel must be a positive integer";
                return false;
            }
            if (options.bits_per_pixel < 1 || options.bits_per_pixel > 3) {
                error_message = "bits_per_pixel must be 1, 2, or 3";
                return false;
            }
        } else if (argument == "--framebuffer") {
            if (index + 1 >= argc) {
                error_message = "missing value for --framebuffer";
                return false;
            }
            options.framebuffer_path = argv[++index];
        } else if (argument == "--port") {
            if (index + 1 >= argc) {
                error_message = "missing value for --port";
                return false;
            }
            if (!parse_u16(argv[++index], options.input_port) ||
                options.input_port == 0) {
                error_message = "port must be an integer in the range 1-65535";
                return false;
            }
        } else if (argument == "--scale") {
            if (index + 1 >= argc) {
                error_message = "missing value for --scale";
                return false;
            }
            if (!parse_u32(argv[++index], options.window_scale) ||
                options.window_scale == 0) {
                error_message = "scale must be a positive integer";
                return false;
            }
        } else if (argument == "--cursor") {
            if (index + 1 >= argc) {
                error_message = "missing value for --cursor";
                return false;
            }
            if (!parse_cursor_mode(argv[++index], options.show_cursor)) {
                error_message = "cursor must be 'on' or 'off'";
                return false;
            }
        } else {
            error_message = "unknown option: " + argument;
            return false;
        }
    }

    if (!has_width) {
        error_message = "missing required argument --width";
        return false;
    }
    if (!has_height) {
        error_message = "missing required argument --height";
        return false;
    }

    return true;
}

std::string build_usage(const char *program_name)
{
    std::ostringstream usage;
    usage << "Usage: " << program_name
          << " --width <width> --height <height> [--bpp <bits_per_pixel>]"
          << " [--framebuffer <path>] [--port <udp_port>]"
          << " [--scale <window_scale>] [--cursor <on|off>]";
    return usage.str();
}
