// Implements a memory-mapped framebuffer file for raster emulation.
// The file is recreated to the packed linear raster size and then mapped
// read-write so another process can update pixels without IPC copies.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 tomaz stih

#include "framebuffer_file.h"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

std::runtime_error make_errno_error(const std::string &message)
{
    return std::runtime_error(message + ": " + std::strerror(errno));
}

} // namespace

framebuffer_file::framebuffer_file(const std::string &path,
    std::size_t size_bytes)
    : path_(path), fd_(-1), size_bytes_(size_bytes), data_(nullptr)
{
    fd_ = open(path_.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd_ < 0) {
        throw make_errno_error("unable to open framebuffer file " + path_);
    }

    if (ftruncate(fd_, static_cast<off_t>(size_bytes_)) != 0) {
        release();
        throw make_errno_error("unable to resize framebuffer file " + path_);
    }

    void *mapping = mmap(nullptr, size_bytes_, PROT_READ | PROT_WRITE,
        MAP_SHARED, fd_, 0);
    if (mapping == MAP_FAILED) {
        data_ = nullptr;
        release();
        throw make_errno_error("unable to mmap framebuffer file " + path_);
    }

    data_ = static_cast<std::uint8_t *>(mapping);
}

framebuffer_file::~framebuffer_file()
{
    release();
}

framebuffer_file::framebuffer_file(framebuffer_file &&other) noexcept
    : path_(std::move(other.path_)),
      fd_(other.fd_),
      size_bytes_(other.size_bytes_),
      data_(other.data_)
{
    other.fd_ = -1;
    other.size_bytes_ = 0;
    other.data_ = nullptr;
}

framebuffer_file &framebuffer_file::operator=(framebuffer_file &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    release();

    path_ = std::move(other.path_);
    fd_ = other.fd_;
    size_bytes_ = other.size_bytes_;
    data_ = other.data_;

    other.fd_ = -1;
    other.size_bytes_ = 0;
    other.data_ = nullptr;

    return *this;
}

std::size_t framebuffer_file::size_bytes() const
{
    return size_bytes_;
}

std::uint8_t *framebuffer_file::data()
{
    return data_;
}

const std::string &framebuffer_file::path() const
{
    return path_;
}

void framebuffer_file::release() noexcept
{
    if (data_ != nullptr) {
        msync(data_, size_bytes_, MS_ASYNC);
        munmap(data_, size_bytes_);
        data_ = nullptr;
    }
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
}

std::size_t calculate_framebuffer_size(std::uint32_t width,
    std::uint32_t height, std::uint32_t bits_per_pixel)
{
    const std::uint64_t bit_count =
        static_cast<std::uint64_t>(width) *
        static_cast<std::uint64_t>(height) *
        static_cast<std::uint64_t>(bits_per_pixel);

    return static_cast<std::size_t>((bit_count + 7U) / 8U);
}
