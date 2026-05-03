// Implements command-line parsing for the rasta SDL2 raster emulator.
// The parser keeps setup minimal while validating packed-pixel formats,
// framebuffer file settings, and UDP event streaming parameters.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 tomaz stih

#include "rasta_options.h"

#include <cstdlib>
#include <cctype>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

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

bool tokenize_argument_string(const std::string &text,
    std::vector<std::string> &tokens, std::string &error_message)
{
    std::string current;
    char quote = '\0';
    bool escape = false;

    for (char ch : text) {
        if (escape) {
            current.push_back(ch);
            escape = false;
            continue;
        }

        if (ch == '\\') {
            escape = true;
            continue;
        }

        if (quote != '\0') {
            if (ch == quote) {
                quote = '\0';
            } else {
                current.push_back(ch);
            }
            continue;
        }

        if (ch == '\'' || ch == '"') {
            quote = ch;
            continue;
        }

        if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }

        current.push_back(ch);
    }

    if (escape) {
        error_message = "unterminated escape in option string";
        return false;
    }
    if (quote != '\0') {
        error_message = "unterminated quote in option string";
        return false;
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return true;
}

bool parse_option_tokens(const std::vector<std::string> &tokens,
    const rasta_options &base_options, bool allow_port_override,
    bool allow_help, rasta_options &options, std::string &error_message)
{
    if (!tokens.empty()) {
        const std::string &first_argument = tokens.front();
        if (first_argument == "--help" || first_argument == "-h") {
            if (allow_help) {
                error_message.clear();
            } else {
                error_message = "help is not supported in subscriber datagrams";
            }
            return false;
        }
    }

    options = base_options;

    for (std::size_t index = 0; index < tokens.size(); ++index) {
        const std::string &argument = tokens[index];
        if (argument == "--width") {
            if (index + 1 >= tokens.size()) {
                error_message = "missing value for --width";
                return false;
            }
            if (!parse_u32(tokens[++index].c_str(), options.width) ||
                options.width == 0U) {
                error_message = "width must be a positive integer";
                return false;
            }
        } else if (argument == "--height") {
            if (index + 1 >= tokens.size()) {
                error_message = "missing value for --height";
                return false;
            }
            if (!parse_u32(tokens[++index].c_str(), options.height) ||
                options.height == 0U) {
                error_message = "height must be a positive integer";
                return false;
            }
        } else if (argument == "--bpp") {
            if (index + 1 >= tokens.size()) {
                error_message = "missing value for --bpp";
                return false;
            }
            if (!parse_u32(tokens[++index].c_str(), options.bits_per_pixel)) {
                error_message = "bits_per_pixel must be a positive integer";
                return false;
            }
            if (options.bits_per_pixel < 1U || options.bits_per_pixel > 3U) {
                error_message = "bits_per_pixel must be 1, 2, or 3";
                return false;
            }
        } else if (argument == "--framebuffer") {
            if (index + 1 >= tokens.size()) {
                error_message = "missing value for --framebuffer";
                return false;
            }
            options.framebuffer_path = tokens[++index];
        } else if (argument == "--port") {
            if (!allow_port_override) {
                error_message = "--port cannot be changed by subscriber datagram";
                return false;
            }
            if (index + 1 >= tokens.size()) {
                error_message = "missing value for --port";
                return false;
            }
            if (!parse_u16(tokens[++index].c_str(), options.input_port) ||
                options.input_port == 0U) {
                error_message = "port must be an integer in the range 1-65535";
                return false;
            }
        } else if (argument == "--scale") {
            if (index + 1 >= tokens.size()) {
                error_message = "missing value for --scale";
                return false;
            }
            if (!parse_u32(tokens[++index].c_str(), options.window_scale) ||
                options.window_scale == 0U) {
                error_message = "scale must be a positive integer";
                return false;
            }
        } else if (argument == "--cursor") {
            if (index + 1 >= tokens.size()) {
                error_message = "missing value for --cursor";
                return false;
            }
            if (!parse_cursor_mode(tokens[++index], options.show_cursor)) {
                error_message = "cursor must be 'on' or 'off'";
                return false;
            }
        } else {
            error_message = "unknown option: " + argument;
            return false;
        }
    }

    return true;
}

} // namespace

rasta_options default_rasta_options()
{
    return rasta_options {
        .width = 320,
        .height = 200,
        .bits_per_pixel = 1,
        .framebuffer_path = "/tmp/rasta.framebuffer",
        .input_port = 5000,
        .window_scale = 4,
        .show_cursor = true,
    };
}

bool parse_arguments(int argc, char **argv, rasta_options &options,
    std::string &error_message)
{
    std::vector<std::string> tokens;
    tokens.reserve(static_cast<std::size_t>(argc > 1 ? argc - 1 : 0));
    for (int index = 1; index < argc; ++index) {
        tokens.push_back(argv[index]);
    }

    return parse_option_tokens(tokens, default_rasta_options(), true, true,
        options, error_message);
}

bool parse_argument_string(const std::string &text,
    const rasta_options &base_options, rasta_options &options,
    std::string &error_message)
{
    std::vector<std::string> tokens;
    if (!tokenize_argument_string(text, tokens, error_message)) {
        return false;
    }

    return parse_option_tokens(tokens, base_options, false, false, options,
        error_message);
}

std::string build_usage(const char *program_name)
{
    std::ostringstream usage;
    usage << "Usage: " << program_name
          << " [--width <width>] [--height <height>]"
          << " [--bpp <bits_per_pixel>]"
          << " [--framebuffer <path>] [--port <udp_port>]"
          << " [--scale <window_scale>] [--cursor <on|off>]";
    return usage.str();
}
