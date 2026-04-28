// Writes a visible test pattern into the rasta mmap framebuffer file.
// This sample acts like a producer process so the SDL emulator can be
// started first and then fed with raster contents from another program.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 tomaz stih

#include "framebuffer_file.h"
#include "raster_decoder.h"
#include "rasta_options.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <string>
#include <thread>

namespace {

void fill_demo_pattern(std::uint8_t *data, std::size_t size_bytes,
    std::uint32_t width, std::uint32_t height, std::uint32_t bits_per_pixel,
    std::uint32_t phase)
{
    std::memset(data, 0, size_bytes);

    const std::uint32_t max_value = (1U << bits_per_pixel) - 1U;
    const std::uint32_t cell_width = width >= 32U ? width / 8U : 4U;
    const std::uint32_t cell_height = height >= 32U ? height / 8U : 4U;

    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            std::uint32_t value = 0;

            if (bits_per_pixel == 1U) {
                const std::uint32_t checker =
                    ((x / cell_width) + (y / cell_height) + phase) % 2U;
                value = checker;
            } else if (bits_per_pixel == 2U) {
                const std::uint32_t band =
                    ((x * 4U) / (width == 0U ? 1U : width) + phase) % 4U;
                value = band;
            } else {
                const std::uint32_t red =
                    (((x / cell_width) + phase) % 2U) << 2U;
                const std::uint32_t green =
                    (((y / cell_height) + phase) % 2U) << 1U;
                const std::uint32_t blue =
                    (((x + y) / (cell_width == 0U ? 1U : cell_width) + phase)
                        % 2U);
                value = red | green | blue;
            }

            set_raster_pixel(data, size_bytes, width, height, bits_per_pixel,
                x, y, value & max_value);
        }
    }
}

void write_demo_frames(const rasta_options &options, std::uint32_t frames,
    std::uint32_t delay_ms)
{
    framebuffer_file framebuffer(options.framebuffer_path,
        calculate_framebuffer_size(options.width, options.height,
            options.bits_per_pixel));

    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        fill_demo_pattern(framebuffer.data(), framebuffer.size_bytes(),
            options.width, options.height, options.bits_per_pixel, frame);
        if (frame + 1U < frames) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(delay_ms));
        }
    }
}

bool parse_u32(const char *text, std::uint32_t &value)
{
    char *end = nullptr;
    const unsigned long parsed = std::strtoul(text, &end, 10);
    if (end == text || *end != '\0') {
        return false;
    }
    value = static_cast<std::uint32_t>(parsed);
    return true;
}

bool parse_demo_arguments(int argc, char **argv, rasta_options &options,
    std::uint32_t &frames, std::uint32_t &delay_ms, std::string &error_message)
{
    options = {
        .width = 0,
        .height = 0,
        .bits_per_pixel = 1,
        .framebuffer_path = "/tmp/rasta.framebuffer",
        .input_port = 5000,
        .window_scale = 4,
        .show_cursor = true,
    };
    frames = 1;
    delay_ms = 120;

    bool has_width = false;
    bool has_height = false;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--width") {
            if (index + 1 >= argc) {
                error_message = "missing value for --width";
                return false;
            }
            if (!parse_u32(argv[++index], options.width) || options.width == 0U) {
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
                options.height == 0U) {
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
            if (options.bits_per_pixel < 1U || options.bits_per_pixel > 3U) {
                error_message = "bits_per_pixel must be 1, 2, or 3";
                return false;
            }
        } else if (argument == "--framebuffer") {
            if (index + 1 >= argc) {
                error_message = "missing value for --framebuffer";
                return false;
            }
            options.framebuffer_path = argv[++index];
        } else if (argument == "--frames") {
            if (index + 1 >= argc) {
                error_message = "missing value for --frames";
                return false;
            }
            if (!parse_u32(argv[++index], frames) || frames == 0U) {
                error_message = "frames must be a positive integer";
                return false;
            }
        } else if (argument == "--delay-ms") {
            if (index + 1 >= argc) {
                error_message = "missing value for --delay-ms";
                return false;
            }
            if (!parse_u32(argv[++index], delay_ms)) {
                error_message = "delay_ms must be a non-negative integer";
                return false;
            }
        } else if (argument == "--port" || argument == "--scale") {
            if (index + 1 >= argc) {
                error_message = "missing value for " + argument;
                return false;
            }
            ++index;
        } else if (argument == "--help" || argument == "-h") {
            return false;
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

std::string build_demo_usage(const char *program_name)
{
    return std::string("Usage: ") + program_name +
        " --width <width> --height <height> [--bpp <bits_per_pixel>]" +
        " [--framebuffer <path>] [--frames <count>] [--delay-ms <ms>]";
}

} // namespace

int main(int argc, char **argv)
{
    rasta_options options {};
    std::uint32_t frames = 1;
    std::uint32_t delay_ms = 120;
    std::string error_message;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--help" || argument == "-h") {
            std::cout << build_demo_usage(argv[0]) << '\n';
            return 0;
        }
    }

    if (!parse_demo_arguments(argc, argv, options, frames, delay_ms,
        error_message)) {
        std::cerr << "error: " << error_message << '\n';
        std::cerr << build_demo_usage(argv[0]) << '\n';
        return 1;
    }

    try {
        write_demo_frames(options, frames, delay_ms);
    } catch (const std::exception &exception) {
        std::cerr << "fatal: " << exception.what() << '\n';
        return 1;
    }

    return 0;
}
