#ifndef LOCHFOLK_STREAM_HPP
#define LOCHFOLK_STREAM_HPP

#pragma once

#include <memory>
#include <iostream>
#include "detail/config.hpp"
#include "io.hpp"

namespace lochfolk
{
/**
 * @brief Input stream over a virtual file
 *
 * The stream is a thin adapter: reads always go through a `file_handle`,
 * never through `std::filebuf` or `std::stringbuf`.
 *
 * `virtual_file_system::open` opens the file for binary reading, so line
 * endings are not translated. Use `virtual_file_system::fopen` with
 * `convert_crlf = true`, or `virtual_file_system::read_string`, when CRLF
 * conversion is wanted.
 *
 * Read errors are reported as `badbit`; end of file is reported as `eofbit`.
 */
class ivfstream final : public std::istream
{
    using my_base = std::istream;

public:
    ivfstream() = delete;

    LOCHFOLK_API ivfstream(ivfstream&& other) noexcept;

    LOCHFOLK_API ivfstream(std::unique_ptr<std::streambuf> buf);

    LOCHFOLK_API ivfstream(std::unique_ptr<file_handle> fh);

    LOCHFOLK_API ~ivfstream();

    /**
     * @brief Returns true if the underlying buffer is available
     */
    [[nodiscard]]
    LOCHFOLK_API bool has_buffer() const noexcept;

private:
    std::unique_ptr<std::streambuf> m_buf;
};
} // namespace lochfolk

#endif
