#ifndef LOCHFOLK_FH_POSIX_HPP
#define LOCHFOLK_FH_POSIX_HPP

#pragma once

#include <lochfolk/detail/config.hpp>
#include <lochfolk/io.hpp>
#include <filesystem>

#ifdef LOCHFOLK_IMPL_POSIX

namespace lochfolk
{
class fh_posix_file : public file_handle
{
public:
    fh_posix_file(const std::filesystem::path&);

    ~fh_posix_file();

    void open(const std::filesystem::path& p);

    void close();

    [[nodiscard]]
    bool is_open() const noexcept;

    [[nodiscard]]
    bool valid() const noexcept override;

    [[nodiscard]]
    file_flag get_flags() const override;

    [[nodiscard]]
    std::uint64_t file_size() const override;

    bool seek(std::int64_t off, whence w) override;

    io_result read(std::span<std::byte> buf, std::uint64_t size) override;

    io_result write(std::span<const std::byte> buf) override;

private:
    int m_fd;
    std::uint64_t m_pos;
};
} // namespace lochfolk

#endif

#endif
