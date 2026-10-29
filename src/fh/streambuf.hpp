#ifndef LOCHFOLK_FH_STREAMBUF_HPP
#define LOCHFOLK_FH_STREAMBUF_HPP

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <streambuf>
#include <lochfolk/io.hpp>

namespace lochfolk
{
/**
 * @brief std::streambuf adapter over a file_handle
 *
 * This is the only bridge between the stream API and the I/O layer: it reads
 * through `file_handle::read` and never uses `std::filebuf`, `std::stringbuf`
 * or any other CRT/standard library file object.
 *
 * Reads are buffered, putback works inside the current buffer (at least one
 * character after `underflow` returned it), and positions follow the raw
 * offsets of the underlying handle. Read errors are reported by throwing
 * `std::ios_base::failure`, which `std::istream` reports as `badbit`.
 */
class fh_streambuf final : public std::streambuf
{
public:
    explicit fh_streambuf(std::unique_ptr<file_handle> fh);

    ~fh_streambuf() override = default;

    fh_streambuf(const fh_streambuf&) = delete;

    fh_streambuf& operator=(const fh_streambuf&) = delete;

protected:
    int_type underflow() override;

    int_type pbackfail(int_type ch) override;

    std::streamsize xsgetn(char_type* s, std::streamsize count) override;

    std::streamsize showmanyc() override;

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
    /**
     * @brief Reads the next chunk from the handle into the buffer
     *
     * Only called when the buffer is exhausted. Precondition: the handle is
     * positioned at @ref m_fill_end.
     *
     * @return `false` on end of file
     * @throw std::ios_base::failure if the underlying read failed
     */
    bool fill();

    /**
     * @brief Offset of the character at `gptr()`
     */
    [[nodiscard]]
    std::uint64_t logical_pos() const;

    static constexpr std::size_t buf_size = 8192;

    std::unique_ptr<file_handle> m_fh;
    std::uint64_t m_fill_end = 0; //< Offset of `egptr()`
    bool m_eof = false;
    char_type m_buf[buf_size];
};
} // namespace lochfolk

#endif
