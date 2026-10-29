/**
 * @file fh/bytes.hpp
 * @author HenryAWE
 * @brief File handles backed by an in-memory byte container
 */

#ifndef LOCHFOLK_FH_BYTES_HPP
#define LOCHFOLK_FH_BYTES_HPP

#pragma once

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>
#include <lochfolk/io.hpp>

namespace lochfolk
{
namespace detail
{
    template <typename T>
    concept byte_like =
        std::same_as<T, std::byte> ||
        std::same_as<T, std::uint8_t> ||
        std::same_as<T, std::int8_t> ||
        std::same_as<T, char>;
} // namespace detail

/**
 * @brief Contiguous container of byte-like elements
 */
template <typename T>
concept const_bytes_container = requires(const T& cv) {
    { std::size(cv) } -> std::convertible_to<std::uint64_t>;
    { std::data(cv) } -> std::convertible_to<const void*>;
} && detail::byte_like<std::remove_cv_t<typename T::value_type>>;

/**
 * @brief Read-only file handle over an owned byte container
 *
 * The container is copied (or moved) into the handle, so the handle never
 * observes memory owned by the file system.
 */
template <const_bytes_container Container>
class fh_bytes final : public file_handle
{
public:
    using container_type = Container;

    explicit fh_bytes(container_type bytes)
        : m_buf(std::move(bytes)) {}

    [[nodiscard]]
    file_flag get_flags() const override
    {
        return file_flag::readable;
    }

    [[nodiscard]]
    std::uint64_t file_size() const override
    {
        return static_cast<std::uint64_t>(std::size(m_buf));
    }

    bool seek(std::int64_t off, whence w) override
    {
        std::uint64_t base = 0;
        switch(w)
        {
        case whence::begin:
            base = 0;
            break;

        case whence::current:
            base = m_pos;
            break;

        case whence::end:
            base = file_size();
            break;
        }

        if(off < 0)
        {
            // Compute the magnitude without negating INT64_MIN.
            const auto delta =
                static_cast<std::uint64_t>(-(off + 1)) + 1;
            if(delta > base)
                return false;

            m_pos = base - delta;
        }
        else
        {
            const auto delta = static_cast<std::uint64_t>(off);
            if(delta > std::numeric_limits<std::uint64_t>::max() - base)
                return false;

            m_pos = base + delta;
        }

        return true;
    }

    io_result read(std::span<std::byte> buf) override
    {
        const std::uint64_t size = file_size();
        if(m_pos >= size || buf.empty())
            return io_result(0);

        const auto count = static_cast<std::size_t>(
            std::min<std::uint64_t>(size - m_pos, buf.size())
        );
        const auto* data = reinterpret_cast<const std::byte*>(std::data(m_buf));
        std::copy_n(data + m_pos, count, buf.data());

        m_pos += count;
        return io_result(count);
    }

    io_result write(std::span<const std::byte>) override
    {
        // Read-only handle
        return io_result();
    }

private:
    container_type m_buf;
    std::uint64_t m_pos = 0;
};

/**
 * @brief Creates a read-only handle owning a copy of @p c
 */
template <const_bytes_container Container>
auto make_fh_bytes(Container&& c)
{
    using container_type = std::remove_cvref_t<Container>;
    return std::make_unique<fh_bytes<container_type>>(
        std::forward<Container>(c)
    );
}
} // namespace lochfolk

#endif
