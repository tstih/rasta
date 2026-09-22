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
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

void fill_demo_pattern(std::uint8_t *data, std::size_t size_bytes,
    std::uint32_t width, std::uint32_t height, std::uint32_t bits_per_pixel,
    std::uint32_t phase)
{
    std::memset(data, 0, size_bytes);

    const std::uint32_t max_value = bits_per_pixel == 32U
        ? 0xffffffffU : (1U << bits_per_pixel) - 1U;
    const std::uint32_t cell_width = width >= 32U ? width / 8U : 4U;
    const std::uint32_t cell_height = height >= 32U ? height / 8U : 4U;

    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            std::uint32_t value = 0;

            if (bits_per_pixel == 32U) {
                const std::uint32_t red = (x + phase) & 0xffU;
                const std::uint32_t green = (y + phase) & 0xffU;
                const std::uint32_t blue = (x + y + phase) & 0xffU;
                value = (blue << 24U) | (green << 16U) |
                    (red << 8U) | 0xffU;
            } else if (bits_per_pixel == 1U) {
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

bool parse_demo_arguments(int argc, char **argv, rasta_options &options,
    std::uint32_t &frames, std::uint32_t &delay_ms, std::string &error_message)
{
    options = default_rasta_options();
    frames = 1;
    delay_ms = 120;
    std::vector<char *> forwarded_arguments;
    forwarded_arguments.reserve(static_cast<std::size_t>(argc));
    forwarded_arguments.push_back(argv[0]);

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--frames") {
            if (index + 1 >= argc) {
                error_message = "missing value for --frames";
                return false;
            }
            std::uint32_t parsed = 0;
            char *end = nullptr;
            const unsigned long value = std::strtoul(argv[++index], &end, 10);
            if (end == argv[index] || *end != '\0') {
                error_message = "frames must be a positive integer";
                return false;
            }
            parsed = static_cast<std::uint32_t>(value);
            if (parsed == 0U) {
                error_message = "frames must be a positive integer";
                return false;
            }
            frames = parsed;
        } else if (argument == "--delay-ms") {
            if (index + 1 >= argc) {
                error_message = "missing value for --delay-ms";
                return false;
            }
            std::uint32_t parsed = 0;
            char *end = nullptr;
            const unsigned long value = std::strtoul(argv[++index], &end, 10);
            if (end == argv[index] || *end != '\0') {
                error_message = "delay_ms must be a non-negative integer";
                return false;
            }
            parsed = static_cast<std::uint32_t>(value);
            delay_ms = parsed;
        } else if (argument == "--help" || argument == "-h") {
            return false;
        } else {
            forwarded_arguments.push_back(argv[index]);
            if ((argument == "--width" || argument == "--height" ||
                 argument == "--bpp" || argument == "--framebuffer" ||
                 argument == "--port" || argument == "--scale" ||
                 argument == "--cursor" || argument == "--inverse") &&
                index + 1 < argc) {
                forwarded_arguments.push_back(argv[++index]);
            }
        }
    }

    return parse_arguments(static_cast<int>(forwarded_arguments.size()),
        forwarded_arguments.data(), options, error_message);
}

std::string build_demo_usage(const char *program_name)
{
    return std::string("Usage: ") + program_name +
        " [--width <width>] [--height <height>] [--bpp <bits_per_pixel>]" +
        " [--framebuffer <path>] [--inverse <on|off>]" +
        " [--frames <count>] [--delay-ms <ms>]";
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
        if (!error_message.empty()) {
            std::cerr << "error: " << error_message << '\n';
        }
        std::cerr << build_demo_usage(argv[0]) << '\n';
        return error_message.empty() ? 0 : 1;
    }

    try {
        write_demo_frames(options, frames, delay_ms);
    } catch (const std::exception &exception) {
        std::cerr << "fatal: " << exception.what() << '\n';
        return 1;
    }

    return 0;
}
