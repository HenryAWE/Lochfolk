#ifndef LOCHFOLK_FH_STREAMBUF_HPP
#define LOCHFOLK_FH_STREAMBUF_HPP

#pragma once

#include <memory>
#include <streambuf>
#include <lochfolk/io.hpp>

namespace lochfolk
{
class fh_streambuf final : public std::streambuf
{
    using my_base = std::streambuf;

public:
    explicit fh_streambuf(std::unique_ptr<file_handle> fh);

    ~fh_streambuf() override = default;

protected:
    int_type underflow() override;

    std::streamsize xsgetn(char_type* s, std::streamsize count) override;

    pos_type seekoff(
        off_type off,
        std::ios_base::seekdir way,
        std::ios_base::openmode which
    ) override;

    pos_type seekpos(
        pos_type pos,
        std::ios_base::openmode which
    ) override;

private:
    static constexpr std::size_t putback_size = 8;
    static constexpr std::size_t buf_size = 8192;

    std::unique_ptr<file_handle> m_fh;
    std::uint64_t m_pos;
    char m_buf[putback_size + buf_size];
};
} // namespace lochfolk

#endif
