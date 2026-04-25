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

private:
    int m_fd;
};
} // namespace lochfolk

#endif

#endif
