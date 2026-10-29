/**
 * @file fh/file.hpp
 * @brief File handle backed by an operating system file
 */

#ifndef LOCHFOLK_FH_FILE_HPP
#define LOCHFOLK_FH_FILE_HPP

#pragma once

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <lochfolk/io.hpp>
#include "platform.hpp"

namespace lochfolk
{
/**
 * @brief Handle to a file on the host file system
 *
 * The requested access modes are honoured: a handle opened with
 * `file_flag::writable` only can be written but not read. Opening a file
 * that does not exist (or cannot be accessed) throws `std::system_error`.
 */
class fh_file final : public file_handle
{
public:
    fh_file(const std::filesystem::path& p, file_flag flags);

    ~fh_file() override;

    fh_file(const fh_file&) = delete;

    fh_file& operator=(const fh_file&) = delete;

    [[nodiscard]]
    file_flag get_flags() const override
    {
        return m_flags;
    }

    [[nodiscard]]
    std::uint64_t file_size() const override;

    bool seek(std::int64_t off, whence w) override;

    io_result read(std::span<std::byte> buf) override;

    io_result write(std::span<const std::byte> buf) override;

private:
#if defined(LOCHFOLK_FH_WIN32)
    void* m_handle = nullptr;
#elif defined(LOCHFOLK_FH_POSIX)
    int m_fd = -1;
#else
    std::FILE* m_file = nullptr;
#endif
    file_flag m_flags = file_flag::unknown;
};
} // namespace lochfolk

#endif
