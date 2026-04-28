// Implements the SDL2 window and render loop for the rasta emulator.
// Each loop iteration polls input, refreshes the subscriber endpoint,
// decodes the packed framebuffer, uploads an RGB texture, and presents.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 tomaz stih

#include "sdl_emulator.h"

#include "raster_decoder.h"

#include <SDL.h>

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class sdl_guard {
public:
    sdl_guard()
    {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
            throw std::runtime_error(SDL_GetError());
        }
    }

    ~sdl_guard()
    {
        SDL_Quit();
    }
};

std::runtime_error make_sdl_error(const std::string &prefix)
{
    return std::runtime_error(prefix + ": " + SDL_GetError());
}

std::uint32_t clamp_zoom(std::uint32_t zoom)
{
    return std::clamp<std::uint32_t>(zoom, 1U, 32U);
}

void apply_zoom(SDL_Window *window, std::uint32_t width, std::uint32_t height,
    std::uint32_t zoom)
{
    const int window_width = static_cast<int>(
        static_cast<std::uint64_t>(width) * zoom);
    const int window_height = static_cast<int>(
        static_cast<std::uint64_t>(height) * zoom);

    SDL_SetWindowSize(window, window_width, window_height);
}

bool is_zoom_in_key(SDL_Keycode keycode)
{
    return keycode == SDLK_EQUALS || keycode == SDLK_PLUS ||
        keycode == SDLK_KP_PLUS;
}

bool is_zoom_out_key(SDL_Keycode keycode)
{
    return keycode == SDLK_MINUS || keycode == SDLK_KP_MINUS;
}

std::string build_window_title(const rasta_options &options,
    std::uint32_t zoom)
{
    std::ostringstream title;
    title << "rasta "
          << options.width << "x" << options.height
          << " " << options.bits_per_pixel << "bpp"
          << " scale=" << zoom
          << " cursor=" << (options.show_cursor ? "on" : "off")
          << " port=" << options.input_port
          << " fb=" << options.framebuffer_path;
    return title.str();
}

SDL_Rect build_destination_rect(SDL_Window *window)
{
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window, &width, &height);

    return SDL_Rect {
        .x = 0,
        .y = 0,
        .w = width,
        .h = height,
    };
}

int scale_coordinate_to_raster(int value, int window_extent,
    std::uint32_t raster_extent)
{
    if (window_extent <= 0 || raster_extent == 0U) {
        return 0;
    }

    const std::int64_t scaled =
        (static_cast<std::int64_t>(value) * raster_extent) / window_extent;
    const std::int64_t max_value = static_cast<std::int64_t>(raster_extent) - 1;
    return static_cast<int>(std::clamp<std::int64_t>(scaled, 0, max_value));
}

void normalize_mouse_event_to_raster(SDL_Window *window,
    const rasta_options &options, SDL_Event &event)
{
    int window_width = 0;
    int window_height = 0;
    SDL_GetWindowSize(window, &window_width, &window_height);

    if (event.type == SDL_MOUSEMOTION) {
        event.motion.x = scale_coordinate_to_raster(event.motion.x, window_width,
            options.width);
        event.motion.y = scale_coordinate_to_raster(event.motion.y, window_height,
            options.height);
    } else if (event.type == SDL_MOUSEBUTTONDOWN ||
               event.type == SDL_MOUSEBUTTONUP) {
        event.button.x = scale_coordinate_to_raster(event.button.x, window_width,
            options.width);
        event.button.y = scale_coordinate_to_raster(event.button.y, window_height,
            options.height);
    }
}

} // namespace

void run_sdl_emulator(const rasta_options &options,
    framebuffer_file &framebuffer, input_broadcaster &broadcaster)
{
    sdl_guard sdl;

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_ShowCursor(options.show_cursor ? SDL_ENABLE : SDL_DISABLE);

    std::uint32_t zoom = clamp_zoom(options.window_scale);
    std::string window_title = build_window_title(options, zoom);
    const int window_width = static_cast<int>(
        static_cast<std::uint64_t>(options.width) * zoom);
    const int window_height = static_cast<int>(
        static_cast<std::uint64_t>(options.height) * zoom);

    SDL_Window *window = SDL_CreateWindow(window_title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_width,
        window_height, 0);
    if (window == nullptr) {
        throw make_sdl_error("unable to create SDL window");
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (renderer == nullptr) {
        SDL_DestroyWindow(window);
        throw make_sdl_error("unable to create SDL renderer");
    }

    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24,
        SDL_TEXTUREACCESS_STREAMING, static_cast<int>(options.width),
        static_cast<int>(options.height));
    if (texture == nullptr) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        throw make_sdl_error("unable to create SDL texture");
    }

    SDL_SetWindowMinimumSize(window, static_cast<int>(options.width),
        static_cast<int>(options.height));
    apply_zoom(window, options.width, options.height, zoom);

    std::vector<std::uint8_t> rgb24_buffer;
    const int pitch = static_cast<int>(options.width * 3U);
    bool running = true;

    while (running) {
        broadcaster.poll_subscribers();

        SDL_Event event {};
        while (SDL_PollEvent(&event) != 0) {
            SDL_Event broadcast_event = event;
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                if (is_zoom_in_key(event.key.keysym.sym)) {
                    zoom = clamp_zoom(zoom + 1U);
                    apply_zoom(window, options.width, options.height, zoom);
                    window_title = build_window_title(options, zoom);
                    SDL_SetWindowTitle(window, window_title.c_str());
                } else if (is_zoom_out_key(event.key.keysym.sym)) {
                    if (zoom > 1U) {
                        zoom = clamp_zoom(zoom - 1U);
                    }
                    apply_zoom(window, options.width, options.height, zoom);
                    window_title = build_window_title(options, zoom);
                    SDL_SetWindowTitle(window, window_title.c_str());
                }
            }
            normalize_mouse_event_to_raster(window, options, broadcast_event);
            broadcaster.broadcast_event(broadcast_event);
        }

        decode_raster_to_rgb24(framebuffer.data(), framebuffer.size_bytes(),
            options.width, options.height, options.bits_per_pixel,
            rgb24_buffer);

        if (SDL_UpdateTexture(texture, nullptr, rgb24_buffer.data(),
            pitch) != 0) {
            SDL_DestroyTexture(texture);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            throw make_sdl_error("unable to update SDL texture");
        }

        if (SDL_RenderClear(renderer) != 0) {
            SDL_DestroyTexture(texture);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            throw make_sdl_error("unable to clear SDL renderer");
        }
        const SDL_Rect destination = build_destination_rect(window);
        if (SDL_RenderCopy(renderer, texture, nullptr, &destination) != 0) {
            SDL_DestroyTexture(texture);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            throw make_sdl_error("unable to draw SDL texture");
        }
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}
