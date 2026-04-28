// Declares a memory-mapped linear framebuffer file for raster emulation.
// The mapped file owns a packed bitstream of pixels that another process
// may write while the emulator continuously displays its contents.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 tomaz stih

#ifndef FRAMEBUFFER_FILE_H
#define FRAMEBUFFER_FILE_H

#include <cstddef>
#include <cstdint>
#include <string>

class framebuffer_file {
public:
    framebuffer_file(const std::string &path, std::size_t size_bytes);
    ~framebuffer_file();

    framebuffer_file(const framebuffer_file &) = delete;
    framebuffer_file &operator=(const framebuffer_file &) = delete;

    framebuffer_file(framebuffer_file &&other) noexcept;
    framebuffer_file &operator=(framebuffer_file &&other) noexcept;

    // Returns the mapped byte size of the framebuffer file.
    std::size_t size_bytes() const;

    // Returns a mutable pointer to the mmap-backed raster data.
    std::uint8_t *data();

    // Returns the filesystem path backing the framebuffer mapping.
    const std::string &path() const;

private:
    void release() noexcept;

    std::string path_;
    int fd_;
    std::size_t size_bytes_;
    std::uint8_t *data_;
};

// Computes the packed framebuffer size for a linear raster.
// Pixels are densely packed with no per-row padding.
std::size_t calculate_framebuffer_size(std::uint32_t width,
    std::uint32_t height, std::uint32_t bits_per_pixel);

#endif
