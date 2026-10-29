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
 * `virtual_file_system::open` uses text mode by default: CRLF is translated
 * to LF on every platform. Pass `std::ios_base::binary` when the raw bytes are
 * needed.
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

    /**
     * @brief Reads from a file handle
     *
     * @param fh Handle to read from
     * @param convert_crlf Translate CRLF to LF while reading
     */
    LOCHFOLK_API ivfstream(
        std::unique_ptr<file_handle> fh, bool convert_crlf = false
    );

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
