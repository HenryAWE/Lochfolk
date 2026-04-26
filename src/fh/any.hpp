/**
 * @file fh/any.hpp
 * @author HenryAWE
 * @brief File handles that are available on any platform
 */

#ifndef LOCHFOLK_FH_ANY_HPP
#define LOCHFOLK_FH_ANY_HPP

#pragma once

#include <memory>
#include <span>
#include <string>
#include <algorithm>
#include <lochfolk/io.hpp>

namespace lochfolk
{
class fh_span final : public file_handle
{
public:
    fh_span(std::span<const std::byte> sv)
        : m_buf(sv)
    {
        m_cursor = m_buf.data();
    }

    fh_span(std::string_view sv)
        : m_buf(reinterpret_cast<const std::byte*>(sv.data()), sv.size())
    {
        m_cursor = m_buf.data();
    }

    file_flag get_flags() const override
    {
        return file_flag::readable;
    }

    std::uint64_t file_size() const override
    {
        return m_buf.size();
    }

    bool seek(std::int64_t off, whence w) override
    {
        std::uint64_t from;
        switch(w)
        {
        default:
        case whence::cur:
            from = m_cursor - m_buf.data();
            break;

        case whence::end:
            from = m_buf.size();
            break;

        case whence::set:
            from = 0;
            break;
        }

        off = static_cast<std::int64_t>(from) + off;
        if(off < 0 || static_cast<std::uint64_t>(off) > m_buf.size())
            return false;

        m_cursor = m_buf.data() + off;
        return true;
    }

    io_result read(std::span<std::byte> buf, std::uint64_t size) override
    {
        size = std::min(buf.size_bytes(), static_cast<std::size_t>(size));

        size = std::min(
            m_buf.size() - (m_cursor - m_buf.data()), static_cast<std::size_t>(size)
        );

        std::copy_n(
            m_cursor, size, buf.begin()
        );
        return io_result(size);
    }

    io_result write(std::span<const std::byte> buf) override
    {
        return io_result(0);
    }

private:
    std::span<const std::byte> m_buf;
    const std::byte* m_cursor;
};

namespace detail
{
    template <typename T>
    concept byte_like =
        std::same_as<T, std::byte> ||
        std::same_as<T, std::uint8_t> ||
        std::same_as<T, std::int8_t> ||
        std::same_as<T, char>;
} // namespace detail

template <typename T>
concept const_bytes_container = requires(const T cv) {
    { std::size(cv) } -> std::convertible_to<std::uint64_t>;
    (const std::byte*)std::data(cv);
} && detail::byte_like<std::remove_cv_t<typename T::value_type>>;

template <const_bytes_container ConstBytesContainer>
class fh_const_bytes : public file_handle
{
public:
    using container_type = ConstBytesContainer;

    fh_const_bytes(container_type str)
        : m_buf(std::move(str))
    {
        m_cursor = (const std::byte*)std::data(m_buf);
    }

    file_flag get_flags() const override
    {
        return file_flag::readable;
    }

    std::uint64_t file_size() const override
    {
        return std::size(m_buf);
    }

    bool seek(std::int64_t off, whence w) override
    {
        std::uint64_t from;
        switch(w)
        {
        default:
        case whence::cur:
            from = current_pos();
            break;

        case whence::end:
            from = buf_size();
            break;

        case whence::set:
            from = 0;
            break;
        }

        off = static_cast<std::int64_t>(from) + off;
        if(off < 0 || static_cast<std::uint64_t>(off) > m_buf.size())
            return false;

        m_cursor = buf_data() + off;
        return true;
    }

    io_result read(std::span<std::byte> buf, std::uint64_t size) override
    {
        size = std::min(buf.size_bytes(), static_cast<std::size_t>(size));
        size = std::min(buf_size() - current_pos(), size);

        std::copy_n(
            m_cursor, size, buf.begin()
        );
        return io_result(size);
    }

    io_result write(std::span<const std::byte> buf) override
    {
        return io_result(0);
    }

private:
    container_type m_buf;
    const std::byte* m_cursor;

    const std::uint64_t current_pos() const
    {
        return m_cursor - buf_data();
    }

    const std::byte* buf_data() const
    {
        return (const std::byte*)std::data(m_buf);
    }

    std::uint64_t buf_size() const
    {
        return std::size(m_buf);
    }
};

template <const_bytes_container ConstBytesContainer>
auto make_unique_const_bytes(ConstBytesContainer&& c)
{
    using container_type = std::remove_cv_t<ConstBytesContainer>;
    return std::make_unique<fh_const_bytes<container_type>>(std::forward<ConstBytesContainer>(c));
}
} // namespace lochfolk

#endif
