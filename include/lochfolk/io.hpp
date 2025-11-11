#ifndef LOCHFOLK_IO_HPP
#define LOCHFOLK_IO_HPP

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include "detail/config.hpp"

namespace lochfolk
{
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

    io_result& operator=(const io_result&) noexcept = default;

    [[nodiscard]]
    constexpr size_type bytes_transferred() const noexcept
    {
        return m_transferred;
    }

    explicit operator size_type() const
    {
        return m_transferred;
    }

    [[nodiscard]]
    bool is_npos() const noexcept
    {
        return m_transferred == npos;
    }

private:
    size_type m_transferred;
};

enum class file_flag : std::int32_t
{
    unknown = 0,

    readable = 1,
    writable = 1 << 1,

    read_write = readable | writable
};

constexpr file_flag operator&(file_flag lhs, file_flag rhs) noexcept
{
    return file_flag(
        static_cast<std::int32_t>(lhs) & static_cast<std::int32_t>(rhs)
    );
}

enum whence : int
{
    cur = 0,
    set = 1,
    end = 2
};

class file_handle
{
protected:
    file_handle();

public:
    file_handle(const file_handle&) = delete;

    file_handle& operator=(const file_handle&) = delete;

    LOCHFOLK_API virtual ~file_handle();

    [[nodiscard]]
    LOCHFOLK_API virtual bool valid() const noexcept;

    [[nodiscard]]
    LOCHFOLK_API virtual file_flag get_flags() const = 0;

    [[nodiscard]]
    bool readable() const
    {
        return static_cast<bool>(get_flags() & file_flag::readable);
    }

    [[nodiscard]]
    bool writable() const
    {
        return static_cast<bool>(get_flags() & file_flag::writable);
    }

    LOCHFOLK_API virtual std::uint64_t file_size() const;

    LOCHFOLK_API virtual bool seek(std::int64_t off, whence w = whence::cur) = 0;
    LOCHFOLK_API virtual io_result read(std::span<std::byte> buf, std::uint64_t size) = 0;
    LOCHFOLK_API virtual io_result write(std::span<const std::byte> buf) = 0;
};

} // namespace lochfolk

#endif
