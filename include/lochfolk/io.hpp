#ifndef LOCHFOLK_IO_HPP
#define LOCHFOLK_IO_HPP

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include "detail/config.hpp"

namespace lochfolk
{
/**
 * @brief Result of a read or write operation
 *
 * A result holds the number of bytes transferred. The special value
 * `io_result::npos` (the default constructed state) reports an error,
 * while a value of 0 reports end-of-file for reads.
 */
class io_result
{
public:
    using size_type = std::uint64_t;

    static constexpr size_type npos = static_cast<size_type>(-1);

    constexpr io_result() noexcept
        : m_transferred(npos) {}

    constexpr io_result(size_type transferred) noexcept
        : m_transferred(transferred) {}

    constexpr io_result(const io_result&) noexcept = default;

    constexpr io_result& operator=(const io_result&) noexcept = default;

    [[nodiscard]]
    constexpr size_type bytes_transferred() const noexcept
    {
        return m_transferred;
    }

    [[nodiscard]]
    constexpr bool is_npos() const noexcept
    {
        return m_transferred == npos;
    }

    [[nodiscard]]
    constexpr bool is_eof() const noexcept
    {
        return m_transferred == 0;
    }

    constexpr explicit operator size_type() const noexcept
    {
        return m_transferred;
    }

private:
    size_type m_transferred;
};

/**
 * @brief Access mode of a file handle
 */
enum class file_flag : std::uint32_t
{
    unknown = 0,

    readable = 1u,
    writable = 1u << 1,

    read_write = readable | writable
};

constexpr file_flag operator|(file_flag lhs, file_flag rhs) noexcept
{
    return static_cast<file_flag>(
        static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs)
    );
}

constexpr file_flag operator&(file_flag lhs, file_flag rhs) noexcept
{
    return static_cast<file_flag>(
        static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs)
    );
}

constexpr file_flag& operator|=(file_flag& lhs, file_flag rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

constexpr file_flag& operator&=(file_flag& lhs, file_flag rhs) noexcept
{
    lhs = lhs & rhs;
    return lhs;
}

[[nodiscard]]
constexpr bool has_flag(file_flag value, file_flag flag) noexcept
{
    return (value & flag) == flag;
}

/**
 * @brief Origin of a seek operation
 */
enum class whence : int
{
    begin = 0,
    current = 1,
    end = 2
};

/**
 * @brief Platform independent file handle
 *
 * A handle is always valid once constructed: implementations either open
 * the underlying resource successfully or throw. `get_flags()` reports the
 * access modes that were actually granted, which may be narrower than the
 * requested ones (string and archive entries are always read-only).
 *
 * @note String and archive handles capture the contents when they are
 * created, so they stay usable after the file system is modified or
 * destroyed.
 */
class LOCHFOLK_API file_handle
{
protected:
    file_handle() = default;

public:
    file_handle(const file_handle&) = delete;

    file_handle& operator=(const file_handle&) = delete;

    virtual ~file_handle();

    /**
     * @brief Access modes granted by this handle
     */
    [[nodiscard]]
    virtual file_flag get_flags() const = 0;

    [[nodiscard]]
    bool readable() const
    {
        return has_flag(get_flags(), file_flag::readable);
    }

    [[nodiscard]]
    bool writable() const
    {
        return has_flag(get_flags(), file_flag::writable);
    }

    /**
     * @brief Size of the underlying data in bytes
     *
     * For handles that convert line endings (see
     * `virtual_file_system::fopen`) this is the raw, untranslated size.
     */
    [[nodiscard]]
    virtual std::uint64_t file_size() const = 0;

    /**
     * @brief Seek to a position
     *
     * @return `false` if the seek failed or the position is invalid
     */
    virtual bool seek(std::int64_t off, whence w = whence::current) = 0;

    /**
     * @brief Read up to `buf.size()` bytes
     *
     * @return Bytes read, 0 on end-of-file, `io_result::npos` on error
     */
    virtual io_result read(std::span<std::byte> buf) = 0;

    /**
     * @brief Write up to `buf.size()` bytes
     *
     * @return Bytes written, `io_result::npos` on error or when the handle
     * is not writable
     */
    virtual io_result write(std::span<const std::byte> buf) = 0;
};

} // namespace lochfolk

#endif
