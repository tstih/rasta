// Declares packed-pixel decoding for the rasta framebuffer emulator.
// The decoder expands 1-bit mono, 2-bit grayscale, and 3-bit RGB data
// into a 24-bit RGB buffer suitable for SDL2 texture uploads.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 tomaz stih

#ifndef RASTER_DECODER_H
#define RASTER_DECODER_H

#include <cstddef>
#include <cstdint>
#include <vector>

// Decodes a packed framebuffer into interleaved RGB24 bytes.
// Supports bits_per_pixel values 1, 2, and 3 only.
void decode_raster_to_rgb24(const std::uint8_t *source,
    std::size_t source_size, std::uint32_t width, std::uint32_t height,
    std::uint32_t bits_per_pixel, std::vector<std::uint8_t> &rgb24_buffer);

// Writes one packed pixel into the framebuffer bitstream.
// Pixels are written MSB-first using the same packing that the decoder
// expects when it reads from the mmap-backed raster file.
void set_raster_pixel(std::uint8_t *target, std::size_t target_size,
    std::uint32_t width, std::uint32_t height, std::uint32_t bits_per_pixel,
    std::uint32_t x, std::uint32_t y, std::uint32_t value);

#endif
