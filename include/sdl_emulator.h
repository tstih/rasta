// Declares the SDL2 front end for the rasta raster emulator.
// The emulator displays a packed mmap-backed framebuffer and forwards
// SDL keyboard and mouse events over UDP to a subscribing client.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 tomaz stih

#ifndef SDL_EMULATOR_H
#define SDL_EMULATOR_H

#include "framebuffer_file.h"
#include "input_broadcaster.h"
#include "rasta_options.h"

// Runs the SDL2 render loop until the window closes or SDL reports an
// unrecoverable error. Throws std::runtime_error on initialization
// failures so the caller can print a terminal-friendly error.
void run_sdl_emulator(const rasta_options &options,
    framebuffer_file &framebuffer, input_broadcaster &broadcaster);

#endif
