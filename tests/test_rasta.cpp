// Exercises core non-SDL logic for the rasta raster emulator.
// These tests validate option parsing and packed framebuffer sizing so
// the project has a lightweight regression harness from the start.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 tomaz stih

#include "framebuffer_file.h"
#include "raster_decoder.h"
#include "rasta_options.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool expect(bool condition, const std::string &message)
{
    if (!condition) {
        std::cerr << "test failure: " << message << '\n';
        return false;
    }
    return true;
}

bool test_framebuffer_size()
{
    return expect(calculate_framebuffer_size(8, 1, 1) == 1,
               "8x1x1 should occupy 1 byte") &&
        expect(calculate_framebuffer_size(3, 1, 2) == 1,
            "3x1x2 should occupy 1 byte") &&
        expect(calculate_framebuffer_size(5, 1, 3) == 2,
            "5x1x3 should occupy 2 bytes");
}

bool test_argument_parsing_success()
{
    const char *argv[] = {
        "rasta",
        "--width",
        "320",
        "--height",
        "200",
        "--bpp",
        "3",
        "--framebuffer",
        "/tmp/example.fb",
        "--port",
        "6000",
        "--scale",
        "2",
        "--cursor",
        "off",
    };

    rasta_options options {};
    std::string error_message;

    const bool ok = parse_arguments(static_cast<int>(std::size(argv)), 
        const_cast<char **>(argv), options, error_message);

    return expect(ok, "valid argument list should parse") &&
        expect(error_message.empty(), "valid parse should not set an error") &&
        expect(options.width == 320, "width should be parsed") &&
        expect(options.height == 200, "height should be parsed") &&
        expect(options.bits_per_pixel == 3, "bpp should be parsed") &&
        expect(options.framebuffer_path == "/tmp/example.fb",
            "framebuffer path should be parsed") &&
        expect(options.input_port == 6000, "port should be parsed") &&
        expect(options.window_scale == 2, "scale should be parsed") &&
        expect(!options.show_cursor, "cursor mode should be parsed");
}

bool test_argument_parsing_default_bpp()
{
    const char *argv[] = {
        "rasta",
        "--width",
        "320",
        "--height",
        "200",
        "--scale",
        "3",
    };

    rasta_options options {};
    std::string error_message;

    const bool ok = parse_arguments(static_cast<int>(std::size(argv)),
        const_cast<char **>(argv), options, error_message);

    return expect(ok, "bpp should default when omitted") &&
        expect(error_message.empty(), "default bpp parse should not error") &&
        expect(options.bits_per_pixel == 1, "default bpp should be 1") &&
        expect(options.window_scale == 3, "flags should still parse") &&
        expect(options.show_cursor, "cursor should default to on");
}

bool test_argument_parsing_failure()
{
    const char *argv[] = {
        "rasta",
        "--width",
        "320",
        "--height",
        "200",
        "--bpp",
        "4",
    };

    rasta_options options {};
    std::string error_message;
    const bool ok = parse_arguments(static_cast<int>(std::size(argv)),
        const_cast<char **>(argv), options, error_message);

    return expect(!ok, "invalid bpp should fail") &&
        expect(error_message == "bits_per_pixel must be 1, 2, or 3",
            "invalid bpp should report a helpful error");
}

bool test_argument_parsing_missing_width()
{
    const char *argv[] = {
        "rasta",
        "--height",
        "200",
    };

    rasta_options options {};
    std::string error_message;
    const bool ok = parse_arguments(static_cast<int>(std::size(argv)),
        const_cast<char **>(argv), options, error_message);

    return expect(!ok, "missing width should fail") &&
        expect(error_message == "missing required argument --width",
            "missing width should report a helpful error");
}

bool test_argument_parsing_invalid_cursor()
{
    const char *argv[] = {
        "rasta",
        "--width",
        "320",
        "--height",
        "200",
        "--cursor",
        "maybe",
    };

    rasta_options options {};
    std::string error_message;
    const bool ok = parse_arguments(static_cast<int>(std::size(argv)),
        const_cast<char **>(argv), options, error_message);

    return expect(!ok, "invalid cursor mode should fail") &&
        expect(error_message == "cursor must be 'on' or 'off'",
            "invalid cursor mode should report a helpful error");
}

bool test_packed_pixel_write_and_decode()
{
    std::vector<std::uint8_t> raster(1, 0);
    std::vector<std::uint8_t> rgb24;

    set_raster_pixel(raster.data(), raster.size(), 2, 1, 3, 0, 0, 7);
    set_raster_pixel(raster.data(), raster.size(), 2, 1, 3, 1, 0, 1);
    decode_raster_to_rgb24(raster.data(), raster.size(), 2, 1, 3, rgb24);

    return expect(rgb24.size() == 6, "decoded rgb24 buffer should have 6 bytes") &&
        expect(rgb24[0] == 255 && rgb24[1] == 255 && rgb24[2] == 255,
            "first pixel should decode to white") &&
        expect(rgb24[3] == 0 && rgb24[4] == 0 && rgb24[5] == 255,
            "second pixel should decode to blue");
}

} // namespace

int main()
{
    if (!test_framebuffer_size()) {
        return EXIT_FAILURE;
    }
    if (!test_argument_parsing_success()) {
        return EXIT_FAILURE;
    }
    if (!test_argument_parsing_default_bpp()) {
        return EXIT_FAILURE;
    }
    if (!test_argument_parsing_failure()) {
        return EXIT_FAILURE;
    }
    if (!test_argument_parsing_missing_width()) {
        return EXIT_FAILURE;
    }
    if (!test_packed_pixel_write_and_decode()) {
        return EXIT_FAILURE;
    }
    if (!test_argument_parsing_invalid_cursor()) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
