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

    path_view& operator=(const path_view&) noexcept = default;

    constexpr bool operator==(const path_view& rhs) const noexcept = default;

    constexpr operator std::string_view() const noexcept
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
    constexpr bool empty() const noexcept
    {
        return m_str.empty();
    }

    [[nodiscard]]
    constexpr bool is_absolute() const noexcept
    {
        if(m_str.empty())
            return false;

        // Check if starts with separator
        if(m_str[0] != separator)
            return false;

        // Check for . or .. components
        std::size_t start = 0;
        std::size_t end = m_str.find(separator);

        while(end != std::string_view::npos)
        {
            std::string_view component = m_str.substr(start, end - start);

            if(component == "." || component == "..")
                return false;

            start = end + 1;
            end = m_str.find(separator, start);
        }

        // Check the last component after the last separator
        std::string_view last_component = std::string_view(m_str).substr(start);
        if(last_component == "." || last_component == "..")
            return false;

        return true;
    }

    [[nodiscard]]
    constexpr path_view parent_path() const noexcept
    {
        if(empty())
            return path_view();

        std::string_view sv = m_str;
        if(sv == "/")
            return path_view(std::string_view("/"));
        if(sv.back() == '/')
            sv.remove_suffix(1);
        std::size_t pos = sv.rfind(separator);
        if(pos == m_str.npos)
            return path_view(m_str);
        if(pos == 0)
            return path_view(std::string_view("/"));

        return path_view(sv.substr(0, pos));
    }

    [[nodiscard]]
    constexpr path_view filename() const noexcept
    {
        if(empty())
            return path_view();
        if(m_str.back() == separator)
            return path_view();
        std::size_t pos = m_str.rfind(separator);
        if(pos == m_str.npos)
            return *this;

        pos += 1; // Remove the separator in the front of result.
        return path_view(m_str.substr(pos));
    }

    [[nodiscard]]
    constexpr path_view extension() const noexcept
    {
        auto result = filename();

        if(result.empty())
            return path_view();

        auto pos = result.m_str.rfind('.');
        if(pos == result.m_str.npos || pos == 0)
            return path_view();

        result.m_str = result.m_str.substr(pos);

        return result;
    }

private:
    std::string_view m_str;
};

class path
{
public:
    using value_type = char;
    using string_type = std::string;

    static constexpr value_type separator = '/';

    constexpr path() noexcept = default;
    constexpr path(const path&) = default;
    constexpr path(path&&) noexcept = default;

    constexpr path(std::string str)
        : m_str(std::move(str)) {}

    constexpr path(const char* cstr)
        : m_str(cstr) {}

    template <std::convertible_to<std::string_view> StringView>
    constexpr path(StringView&& sv)
        : m_str(static_cast<std::string_view>(sv))
    {}

    constexpr path(const path_view& pv)
        : m_str(pv) {}

    constexpr path& operator=(const path&) = default;
    constexpr path& operator=(path&&) = default;

    friend constexpr bool operator==(const path& lhs, const path& rhs) noexcept
    {
        return path_view(lhs) == path_view(rhs);
    }

    friend constexpr bool operator==(const path& lhs, const path_view& rhs) noexcept
    {
        return path_view(lhs) == rhs;
    }

    friend constexpr bool operator==(const path_view& lhs, const path& rhs) noexcept
    {
        return lhs == path_view(rhs);
    }

    [[nodiscard]]
    constexpr const char* c_str() const
    {
        return m_str.c_str();
    }

    [[nodiscard]]
    constexpr const std::string& string() const
    {
        return m_str;
    }

    constexpr operator std::string_view() const noexcept
    {
        return m_str;
    }

    constexpr operator path_view() const noexcept
    {
        return path_view(m_str);
    }

    [[nodiscard]]
    constexpr bool empty() const
    {
        return m_str.empty();
    }

    [[nodiscard]]
    constexpr bool is_absolute() const
    {
        return path_view(*this).is_absolute();
    }

    [[nodiscard]]
    constexpr path parent_path() const
    {
        return path_view(*this).parent_path();
    }

    [[nodiscard]]
    constexpr path filename() const
    {
        return path_view(*this).filename();
    }

    [[nodiscard]]
    constexpr path extension() const
    {
        return path_view(*this).extension();
    }

    constexpr auto split_view() const
    {
        return detail::path_split_view(m_str) |
               std::views::transform([](auto&& subrange)
                                     { return path_view(std::string_view(subrange.begin(), subrange.end())); });
    }

    constexpr path& append(path_view p)
    {
        if(p.empty())
            return *this;
        else if(p.is_absolute())
        { // p is absolute
            *this = p;
            return *this;
        }
        else // p is relative
        {
            std::string_view sv = static_cast<std::string_view>(p);
            if(m_str.empty())
                m_str = sv;
            else if(m_str.back() == separator)
            {
                m_str += sv;
            }
            else
            {
                m_str.reserve(m_str.size() + 1 + sv.size());
                m_str += separator;
                m_str.append(std::string_view(p));
            }

            return *this;
        }
    }

    constexpr path& append(std::string_view p)
    {
        return append(path_view(p));
    }

    constexpr path& operator/=(path_view p)
    {
        return append(p);
    }

    constexpr path& operator/=(std::string_view p)
    {
        return append(p);
    }

    friend constexpr path operator/(const path& lhs, path_view rhs)
    {
        path result(lhs);
        result /= rhs;
        return result;
    }

    friend constexpr path operator/(const path& lhs, std::string_view rhs)
    {
        path result(lhs);
        result /= rhs;
        return result;
    }

    template <std::convertible_to<std::string_view> StringView>
    constexpr path& concat(StringView&& sv)
    {
        m_str.append(path_view(std::move(sv)));
        return *this;
    }

    constexpr path& concat(const path& p)
    {
        concat(p.string());
        return *this;
    }

    constexpr path& operator+=(const path& p)
    {
        return concat(p);
    }

    template <std::convertible_to<std::string_view> StringView>
    constexpr path& operator+=(StringView&& sv)
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
