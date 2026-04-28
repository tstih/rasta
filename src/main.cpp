// Starts the rasta SDL2 raster emulator from the command line.
// The program validates raster options, creates an mmap framebuffer
// file, opens a UDP input endpoint, and enters the SDL render loop.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 tomaz stih

#include "framebuffer_file.h"
#include "input_broadcaster.h"
#include "rasta_options.h"
#include "sdl_emulator.h"

#include <exception>
#include <iostream>

int main(int argc, char **argv)
{
    rasta_options options {};
    std::string error_message;

    if (!parse_arguments(argc, argv, options, error_message)) {
        if (!error_message.empty()) {
            std::cerr << "error: " << error_message << '\n';
        }
        std::cerr << build_usage(argv[0]) << '\n';
        return error_message.empty() ? 0 : 1;
    }

    try {
        framebuffer_file framebuffer(options.framebuffer_path,
            calculate_framebuffer_size(options.width, options.height,
                options.bits_per_pixel));
        input_broadcaster broadcaster(options.input_port);

        std::cout << "framebuffer=" << framebuffer.path()
                  << " size=" << framebuffer.size_bytes()
                  << " bytes udp_port=" << broadcaster.port() << '\n';

        run_sdl_emulator(options, framebuffer, broadcaster);
    } catch (const std::exception &exception) {
        std::cerr << "fatal: " << exception.what() << '\n';
        return 1;
    }

    return 0;
}
