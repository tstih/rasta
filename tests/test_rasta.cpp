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
        "--inverse",
        "on",
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
        expect(!options.show_cursor, "cursor mode should be parsed") &&
        expect(options.inverse_pixels, "inverse mode should be parsed");
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
        expect(options.show_cursor, "cursor should default to on") &&
        expect(!options.inverse_pixels, "inverse should default to off");
}

bool test_argument_parsing_cef_bpp()
{
    const char *argv[] = {"rasta", "--bpp", "32"};
    rasta_options options {};
    std::string error_message;
    const bool ok = parse_arguments(static_cast<int>(std::size(argv)),
        const_cast<char **>(argv), options, error_message);

    return expect(ok, "CEF 32bpp should parse") &&
        expect(options.bits_per_pixel == 32,
            "CEF bpp should be retained") &&
        expect(error_message.empty(), "CEF bpp parse should not error");
}

bool test_argument_parsing_default_dimensions()
{
    const char *argv[] = {
        "rasta",
    };

    rasta_options options {};
    std::string error_message;

    const bool ok = parse_arguments(static_cast<int>(std::size(argv)),
        const_cast<char **>(argv), options, error_message);

    return expect(ok, "default startup options should parse") &&
        expect(error_message.empty(), "default parse should not error") &&
        expect(options.width == 320, "default width should be 320") &&
        expect(options.height == 200, "default height should be 200") &&
        expect(options.bits_per_pixel == 1, "default bpp should be 1") &&
        expect(!options.inverse_pixels, "default inverse should be off");
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
        expect(error_message == "bits_per_pixel must be 1, 2, 3, or 32",
            "invalid bpp should report a helpful error");
}

bool test_cef_32bpp_decode()
{
    const std::vector<std::uint8_t> bgra = {
        0x10, 0x20, 0x30, 0x40,
        0xa0, 0xb0, 0xc0, 0xd0,
    };
    std::vector<std::uint8_t> decoded;
    decode_raster_to_bgra32(bgra.data(), bgra.size(), 2, 1, false, decoded);

    return expect(decoded == bgra,
        "CEF BGRA32 bytes should decode without channel reordering");
}

bool test_cef_32bpp_inverse_preserves_alpha()
{
    const std::vector<std::uint8_t> bgra = {0x10, 0x20, 0x30, 0x40};
    std::vector<std::uint8_t> decoded;
    decode_raster_to_bgra32(bgra.data(), bgra.size(), 1, 1, true, decoded);

    return expect(decoded.size() == 4 && decoded[0] == 0xef &&
        decoded[1] == 0xdf && decoded[2] == 0xcf && decoded[3] == 0x40,
        "CEF inverse mode should preserve alpha");
}

bool test_argument_string_reconfigure_success()
{
    rasta_options base_options = default_rasta_options();
    base_options.width = 320;
    base_options.height = 200;
    base_options.window_scale = 6;
    base_options.framebuffer_path = "/tmp/original.fb";

    rasta_options options = base_options;
    std::string error_message;
    const bool ok = parse_argument_string(
        "--width 640 --height 480 --cursor off --inverse on "
        "--framebuffer \"/tmp/updated framebuffer.fb\"",
        base_options, options, error_message);

    return expect(ok, "subscriber option string should parse") &&
        expect(error_message.empty(),
            "subscriber parse should not report an error") &&
        expect(options.width == 640, "reconfigure width should update") &&
        expect(options.height == 480, "reconfigure height should update") &&
        expect(options.window_scale == 6,
            "unspecified scale should be preserved") &&
        expect(options.framebuffer_path ==
            "/tmp/updated framebuffer.fb",
            "quoted framebuffer path should parse") &&
        expect(!options.show_cursor, "cursor override should apply") &&
        expect(options.inverse_pixels, "inverse override should apply");
}

bool test_argument_string_reconfigure_rejects_port()
{
    rasta_options options = default_rasta_options();
    std::string error_message;
    const bool ok = parse_argument_string("--port 6000", options, options,
        error_message);

    return expect(!ok, "subscriber should not be able to change port") &&
        expect(error_message ==
            "--port cannot be changed by subscriber datagram",
            "subscriber port override should report a helpful error");
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

bool test_argument_parsing_invalid_inverse()
{
    const char *argv[] = {
        "rasta",
        "--inverse",
        "maybe",
    };

    rasta_options options {};
    std::string error_message;
    const bool ok = parse_arguments(static_cast<int>(std::size(argv)),
        const_cast<char **>(argv), options, error_message);

    return expect(!ok, "invalid inverse mode should fail") &&
        expect(error_message == "inverse must be 'on' or 'off'",
            "invalid inverse mode should report a helpful error");
}

bool test_packed_pixel_write_and_decode()
{
    std::vector<std::uint8_t> raster(1, 0);
    std::vector<std::uint8_t> rgb24;

    set_raster_pixel(raster.data(), raster.size(), 2, 1, 3, 0, 0, 7);
    set_raster_pixel(raster.data(), raster.size(), 2, 1, 3, 1, 0, 1);
    decode_raster_to_rgb24(raster.data(), raster.size(), 2, 1, 3, false,
        rgb24);

    return expect(rgb24.size() == 6, "decoded rgb24 buffer should have 6 bytes") &&
        expect(rgb24[0] == 255 && rgb24[1] == 255 && rgb24[2] == 255,
            "first pixel should decode to white") &&
        expect(rgb24[3] == 0 && rgb24[4] == 0 && rgb24[5] == 255,
            "second pixel should decode to blue");
}

bool test_packed_pixel_decode_inverse()
{
    std::vector<std::uint8_t> raster(1, 0);
    std::vector<std::uint8_t> rgb24;

    set_raster_pixel(raster.data(), raster.size(), 2, 1, 1, 0, 0, 1);
    set_raster_pixel(raster.data(), raster.size(), 2, 1, 1, 1, 0, 0);
    decode_raster_to_rgb24(raster.data(), raster.size(), 2, 1, 1, true,
        rgb24);

    return expect(rgb24.size() == 6,
               "inverse decode buffer should have 6 bytes") &&
        expect(rgb24[0] == 0 && rgb24[1] == 0 && rgb24[2] == 0,
            "inverted 1bpp set bit should decode to black") &&
        expect(rgb24[3] == 255 && rgb24[4] == 255 && rgb24[5] == 255,
            "inverted 1bpp clear bit should decode to white");
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
    if (!test_argument_parsing_cef_bpp()) {
        return EXIT_FAILURE;
    }
    if (!test_argument_parsing_default_dimensions()) {
        return EXIT_FAILURE;
    }
    if (!test_argument_parsing_failure()) {
        return EXIT_FAILURE;
    }
    if (!test_argument_string_reconfigure_success()) {
        return EXIT_FAILURE;
    }
    if (!test_argument_string_reconfigure_rejects_port()) {
        return EXIT_FAILURE;
    }
    if (!test_packed_pixel_write_and_decode()) {
        return EXIT_FAILURE;
    }
    if (!test_packed_pixel_decode_inverse()) {
        return EXIT_FAILURE;
    }
    if (!test_cef_32bpp_decode()) {
        return EXIT_FAILURE;
    }
    if (!test_cef_32bpp_inverse_preserves_alpha()) {
        return EXIT_FAILURE;
    }
    if (!test_argument_parsing_invalid_cursor()) {
        return EXIT_FAILURE;
    }
    if (!test_argument_parsing_invalid_inverse()) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
