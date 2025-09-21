#ifndef LOCHFOLK_PATH_HPP
#define LOCHFOLK_PATH_HPP

#pragma once

#include <concepts>
#include <string>
#include <ranges>
#include "detail/config.hpp"

namespace lochfolk
{
class path_view;
class path;

namespace detail
{
    constexpr auto path_split_view(std::string_view sv) noexcept
    {
        using namespace std;
        return sv |
               views::split('/') |
               views::filter(
                   [](auto&& subrange) -> bool
                   { return !ranges::empty(subrange); }
               );
    }
} // namespace detail

class path_view
{
public:
    using value_type = char;
    using string_type = std::string;

    static constexpr value_type separator = '/';

    constexpr path_view() noexcept = default;
    constexpr path_view(const path_view&) noexcept = default;

    template <std::convertible_to<std::string_view> StringView>
    constexpr explicit path_view(StringView&& sv)
        : m_str(static_cast<std::string_view>(sv))
    {}

    ~path_view() = default;

    path_view& operator=(const path_view&) noexcept = default;

    bool operator==(const path_view& rhs) const noexcept = default;

    [[nodiscard]]
    std::string string() const noexcept
    {
        return std::string(m_str);
    }

    explicit operator std::string_view() const noexcept
    {
        return m_str;
    }

    [[nodiscard]]
    constexpr auto split_view() const noexcept
    {
        return detail::path_split_view(m_str) |
               std::views::transform(
                   [](auto&& subrange)
                   { return path_view(std::string_view(subrange.begin(), subrange.end())); }
               );
    }

    [[nodiscard]]
    LOCHFOLK_API bool empty() const noexcept;

    [[nodiscard]]
    LOCHFOLK_API bool is_absolute() const noexcept;

    [[nodiscard]]
    LOCHFOLK_API path_view parent_path() const noexcept;

    [[nodiscard]]
    LOCHFOLK_API path_view filename() const noexcept;

    [[nodiscard]]
    LOCHFOLK_API path_view extension() const noexcept;

private:
    std::string_view m_str;
};

class path
{
public:
    using value_type = char;
    using string_type = std::string;

    static constexpr value_type separator = '/';

    LOCHFOLK_API path() noexcept;
    LOCHFOLK_API path(const path&);
    LOCHFOLK_API path(path&&) noexcept;

    LOCHFOLK_API path(std::string str);

    LOCHFOLK_API path(const char* cstr);

    template <std::convertible_to<std::string_view> StringView>
    constexpr path(StringView&& sv)
        : path(std::string(sv))
    {}

    LOCHFOLK_API path(const path_view& pv);

    LOCHFOLK_API ~path();

    LOCHFOLK_API path& operator=(const path&);
    LOCHFOLK_API path& operator=(path&&) noexcept;

    LOCHFOLK_API bool operator==(const path& rhs) const noexcept;

    friend bool operator==(const path& lhs, const path_view& rhs) noexcept
    {
        return lhs.view() == rhs;
    }

    friend bool operator==(const path_view& lhs, const path& rhs) noexcept
    {
        return lhs == rhs.view();
    }

    [[nodiscard]]
    const char* c_str() const
    {
        return m_str.c_str();
    }

    [[nodiscard]]
    const std::string& string() const
    {
        return m_str;
    }

    explicit operator std::string_view() const noexcept
    {
        return m_str;
    }

    LOCHFOLK_API path_view view() const noexcept;

    operator path_view() const noexcept
    {
        return view();
    }

    [[nodiscard]]
    LOCHFOLK_API bool empty() const;

    [[nodiscard]]
    LOCHFOLK_API bool is_absolute() const;

    [[nodiscard]]
    LOCHFOLK_API path parent_path() const;

    [[nodiscard]]
    LOCHFOLK_API path filename() const;

    [[nodiscard]]
    LOCHFOLK_API path extension() const;

    constexpr auto split_view() const
    {
        return detail::path_split_view(m_str) |
               std::views::transform([](auto&& subrange)
                                     { return path_view(std::string_view(subrange.begin(), subrange.end())); });
    }

    LOCHFOLK_API path& append(path_view p);

    path& append(std::string_view p)
    {
        return append(path_view(p));
    }

    path& operator/=(path_view p)
    {
        return append(p);
    }

    path& operator/=(std::string_view p)
    {
        return append(p);
    }

    friend path operator/(const path& lhs, path_view rhs)
    {
        path result(lhs);
        result /= rhs;
        return result;
    }

    friend path operator/(const path& lhs, std::string_view rhs)
    {
        path result(lhs);
        result /= rhs;
        return result;
    }

    LOCHFOLK_API path& concat(std::string_view sv);

    template <std::convertible_to<std::string_view> StringView>
    path& concat(StringView&& sv)
    {
        return concat(std::string_view(sv));
    }

    path& concat(const path& p)
    {
        concat(p.string());
        return *this;
    }

    path& operator+=(const path& p)
    {
        return concat(p);
    }

    template <std::convertible_to<std::string_view> StringView>
    path& operator+=(StringView&& sv)
    {
        return concat(std::move(sv));
    }

    [[nodiscard]]
    LOCHFOLK_API path lexically_normal() const;

private:
    std::string m_str;
};

inline namespace vfs_literals
{
    constexpr path_view operator""_pv(const char* str, std::size_t sz)
    {
        return path_view(std::string_view(str, sz));
    }
} // namespace vfs_literals
} // namespace lochfolk

#endif
