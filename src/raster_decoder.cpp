// Implements packed-pixel raster decoding for the rasta emulator.
// The decoder reads pixels MSB-first from the mmap-backed bitstream and
// expands them into RGB24 for the SDL texture upload path.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 tomaz stih

#include "raster_decoder.h"

#include <algorithm>
#include <stdexcept>

namespace {

class bit_reader {
public:
    bit_reader(const std::uint8_t *data, std::size_t size_bytes)
        : data_(data), size_bytes_(size_bytes), bit_offset_(0)
    {
    }

    std::uint32_t read_bits(std::uint32_t count)
    {
        std::uint32_t value = 0;
        for (std::uint32_t index = 0; index < count; ++index) {
            value <<= 1U;
            if (bit_offset_ < size_bytes_ * 8U) {
                const std::size_t byte_index = bit_offset_ / 8U;
                const std::size_t bit_index = 7U - (bit_offset_ % 8U);
                value |= static_cast<std::uint32_t>(
                    (data_[byte_index] >> bit_index) & 0x1U);
            }
            ++bit_offset_;
        }
        return value;
    }

private:
    const std::uint8_t *data_;
    std::size_t size_bytes_;
    std::size_t bit_offset_;
};

std::uint8_t expand_grayscale(std::uint32_t value, std::uint32_t max_value)
{
    return static_cast<std::uint8_t>((255U * value) / max_value);
}

} // namespace

void decode_raster_to_rgb24(const std::uint8_t *source,
    std::size_t source_size, std::uint32_t width, std::uint32_t height,
    std::uint32_t bits_per_pixel, std::vector<std::uint8_t> &rgb24_buffer)
{
    if (bits_per_pixel < 1 || bits_per_pixel > 3) {
        throw std::runtime_error("unsupported bits_per_pixel");
    }

    const std::size_t pixel_count =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    rgb24_buffer.resize(pixel_count * 3U);

    bit_reader reader(source, source_size);
    for (std::size_t pixel_index = 0; pixel_index < pixel_count; ++pixel_index) {
        const std::size_t output_index = pixel_index * 3U;
        const std::uint32_t value = reader.read_bits(bits_per_pixel);

        if (bits_per_pixel == 3U) {
            rgb24_buffer[output_index + 0U] =
                (value & 0x4U) != 0U ? 255U : 0U;
            rgb24_buffer[output_index + 1U] =
                (value & 0x2U) != 0U ? 255U : 0U;
            rgb24_buffer[output_index + 2U] =
                (value & 0x1U) != 0U ? 255U : 0U;
        } else {
            const std::uint8_t level =
                expand_grayscale(value, (1U << bits_per_pixel) - 1U);
            rgb24_buffer[output_index + 0U] = level;
            rgb24_buffer[output_index + 1U] = level;
            rgb24_buffer[output_index + 2U] = level;
        }
    }
}

void set_raster_pixel(std::uint8_t *target, std::size_t target_size,
    std::uint32_t width, std::uint32_t height, std::uint32_t bits_per_pixel,
    std::uint32_t x, std::uint32_t y, std::uint32_t value)
{
    if (bits_per_pixel < 1 || bits_per_pixel > 3) {
        throw std::runtime_error("unsupported bits_per_pixel");
    }
    if (x >= width || y >= height) {
        throw std::runtime_error("pixel coordinate out of range");
    }

    const std::uint32_t max_value = (1U << bits_per_pixel) - 1U;
    value = std::min(value, max_value);

    const std::size_t pixel_index =
        static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + x;
    const std::size_t start_bit = pixel_index * bits_per_pixel;

    for (std::uint32_t bit = 0; bit < bits_per_pixel; ++bit) {
        const std::size_t absolute_bit = start_bit + bit;
        const std::size_t byte_index = absolute_bit / 8U;
        const std::size_t bit_index = 7U - (absolute_bit % 8U);
        if (byte_index >= target_size) {
            throw std::runtime_error("packed pixel write out of range");
        }

        const std::uint8_t mask =
            static_cast<std::uint8_t>(1U << bit_index);
        const std::uint32_t source_bit =
            (value >> (bits_per_pixel - bit - 1U)) & 0x1U;

        if (source_bit != 0U) {
            target[byte_index] |= mask;
        } else {
            target[byte_index] &= static_cast<std::uint8_t>(~mask);
        }
    }
}
