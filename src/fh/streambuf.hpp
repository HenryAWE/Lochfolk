#ifndef LOCHFOLK_FH_STREAMBUF_HPP
#define LOCHFOLK_FH_STREAMBUF_HPP

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <streambuf>
#include <vector>
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
 * Reads are buffered and putback works inside the current buffer. File
 * positions are always raw byte offsets of the underlying handle, which
 * matches what a text mode file stream reports: with CRLF translation enabled
 * a CRLF pair counts as two raw bytes but produces a single character, so
 * `tellg()` may advance by two for one character.
 *
 * Read errors are reported by throwing `std::ios_base::failure`, which
 * `std::istream` reports as `badbit`.
 */
class fh_streambuf final : public std::streambuf
{
public:
    /**
     * @param fh Handle to read from
     * @param convert_crlf Translate CRLF to LF while reading
     */
    explicit fh_streambuf(
        std::unique_ptr<file_handle> fh, bool convert_crlf = false
    );

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
     * @brief Reads and translates the next chunk into the buffer
     *
     * Only called when the buffer is exhausted. Precondition: the handle is
     * positioned at @ref m_raw_read_pos.
     *
     * @return `false` on end of file
     * @throw std::ios_base::failure if the underlying read failed
     */
    bool fill();

    /**
     * @brief Raw offset of the character at `gptr()`
     */
    [[nodiscard]]
    std::uint64_t logical_pos() const;

    static constexpr std::size_t buf_size = 8192;

    std::unique_ptr<file_handle> m_fh;
    bool m_convert = false;

    std::uint64_t m_raw_read_pos = 0; //< Raw offset of the next read
    std::uint64_t m_raw_begin = 0;    //< Raw offset of the byte at `eback()`

    /**
     * @brief Output indices of the LFs that replaced a CRLF pair
     *
     * Position `i` of the buffer corresponds to raw offset
     * `m_raw_begin + i + (number of entries smaller than i)`.
     */
    std::vector<std::size_t> m_pairs;

    bool m_pending_cr = false;
    bool m_eof = false;
    char_type m_buf[buf_size];
};
} // namespace lochfolk

#endif
