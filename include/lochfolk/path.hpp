#ifndef LOCHFOLK_PATH_HPP
#define LOCHFOLK_PATH_HPP

#pragma once

#include <iosfwd>
#include <concepts>
#include <string>
#include <iterator>
#include "detail/config.hpp"

namespace lochfolk
{
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

    class const_iterator
    {
        friend path_view;

        const_iterator(std::size_t pos, std::size_t len, path_view pv) noexcept;

    public:
        using value_type = path_view;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::bidirectional_iterator_tag;

        const_iterator() noexcept = default;
        const_iterator(const const_iterator&) noexcept = default;

        const_iterator& operator=(const const_iterator&) noexcept = default;

        LOCHFOLK_API bool operator==(const const_iterator& rhs) const noexcept;

        LOCHFOLK_API path_view operator*() const;

        const_iterator& operator++()
        {
            this->next();
            return *this;
        }

        const_iterator operator++(int)
        {
            const_iterator tmp(*this);
            ++*this;
            return tmp;
        }

        const_iterator& operator--()
        {
            this->prev();
            return *this;
        }

        const_iterator operator--(int)
        {
            const_iterator tmp(*this);
            --*this;
            return tmp;
        }

    private:
        LOCHFOLK_API void next();
        LOCHFOLK_API void prev();

        std::size_t m_pos = 0;
        std::size_t m_len = 0;
        std::string_view m_sv;
    };

    [[nodiscard]]
    LOCHFOLK_API const_iterator cbegin() const;
    [[nodiscard]]
    LOCHFOLK_API const_iterator cend() const;

    using iterator = const_iterator;

    iterator begin() const
    {
        return cbegin();
    }

    iterator end() const
    {
        return cend();
    }

    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using reverse_iterator = const_reverse_iterator;

    const_reverse_iterator crbegin() const
    {
        return const_reverse_iterator(end());
    }

    const_reverse_iterator crend() const
    {
        return const_reverse_iterator(begin());
    }

    reverse_iterator rbegin() const
    {
        return crbegin();
    }

    reverse_iterator rend() const
    {
        return crend();
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

LOCHFOLK_API std::ostream& operator<<(std::ostream& os, const path_view& pv);

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

    class const_iterator
    {
        friend path;

        using underlying_type = path_view::const_iterator;

        explicit const_iterator(const underlying_type& other) noexcept;

    public:
        using value_type = path_view;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::bidirectional_iterator_tag;

        const_iterator() noexcept;
        const_iterator(const const_iterator&) noexcept = default;

        bool operator==(const const_iterator& rhs) const noexcept = default;

        path_view operator*() const
        {
            return m_base.operator*();
        }

        const_iterator& operator++()
        {
            m_base.operator++();
            return *this;
        }

        const_iterator operator++(int)
        {
            const_iterator tmp(*this);
            ++*this;
            return tmp;
        }

        const_iterator& operator--()
        {
            m_base.operator--();
            return *this;
        }

        const_iterator operator--(int)
        {
            const_iterator tmp(*this);
            --*this;
            return tmp;
        }

    private:
        underlying_type m_base;
    };

    [[nodiscard]]
    LOCHFOLK_API const_iterator cbegin() const;
    [[nodiscard]]
    LOCHFOLK_API const_iterator cend() const;

    using iterator = const_iterator;

    iterator begin() const
    {
        return cbegin();
    }

    iterator end() const
    {
        return cend();
    }

    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using reverse_iterator = const_reverse_iterator;

    const_reverse_iterator crbegin() const
    {
        return const_reverse_iterator(end());
    }

    const_reverse_iterator crend() const
    {
        return const_reverse_iterator(begin());
    }

    reverse_iterator rbegin() const
    {
        return crbegin();
    }

    reverse_iterator rend() const
    {
        return crend();
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

LOCHFOLK_API std::ostream& operator<<(std::ostream& os, const path& p);

inline namespace vfs_literals
{
    constexpr path_view operator""_pv(const char* str, std::size_t sz)
    {
        return path_view(std::string_view(str, sz));
    }
} // namespace vfs_literals
} // namespace lochfolk

#endif
