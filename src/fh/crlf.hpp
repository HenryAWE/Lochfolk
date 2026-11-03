/**
 * @file fh/crlf.hpp
 * @brief CRLF to LF conversion filter over another file handle
 */

#ifndef LOCHFOLK_FH_CRLF_HPP
#define LOCHFOLK_FH_CRLF_HPP

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <lochfolk/io.hpp>

namespace lochfolk
{
/**
 * @brief Translates CRLF sequences into LF while reading
 *
 * Only reads are translated. Writes are forwarded unchanged, because
 * turning LF into CRLF on write is not well defined for binary payloads.
 *
 * `seek()` operates on the underlying (raw) offsets and drops the
 * translation state, and `file_size()` reports the raw size.
 */
class fh_crlf final : public file_handle
{
public:
    explicit fh_crlf(std::unique_ptr<file_handle> inner);

    [[nodiscard]]
    file_flag get_flags() const override;

    [[nodiscard]]
    std::uint64_t file_size() const override;

    bool seek(std::int64_t off, whence w) override;

    io_result read(std::span<std::byte> buf) override;

    io_result write(std::span<const std::byte> buf) override;

private:
    /**
     * @brief Reads raw bytes from the inner handle and translates them
     *
     * @return `false` if the inner read failed
     */
    bool refill();

    static constexpr std::size_t chunk_size = 4096;

    std::unique_ptr<file_handle> m_inner;

    /**
     * @brief Translated output; one byte larger than a raw chunk
     *
     * A CR held back from the previous chunk can be emitted in addition to
     * the bytes of the current chunk.
     */
    std::array<std::byte, chunk_size + 1> m_buf;
    std::size_t m_begin = 0;
    std::size_t m_end = 0;
    bool m_pending_cr = false;
    bool m_eof = false;
};
} // namespace lochfolk

#endif
