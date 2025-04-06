#ifndef LOCHFOLK_UTILITY_HPP
#define LOCHFOLK_UTILITY_HPP

#pragma once

#include <utility>
#include <limits>
#include <ios>
#include <streambuf>
#include <span>

namespace lochfolk
{
/**
 * @brief Backport of `std::spanbuf` from C++23
 */
class span_buf : public std::streambuf
{
    using my_base = std::streambuf;

    // Code mainly comes from the MSVC STL,
    // which is licensed under Apache-2.0 WITH LLVM-exception
    // See: https://github.com/microsoft/STL/blob/main/stl/inc/spanstream

public:
    using char_type = char;
    using traits_type = std::char_traits<char>;
    using int_type = traits_type::int_type;
    using pos_type = traits_type::pos_type;
    using off_type = traits_type::off_type;

    span_buf() = default;

    explicit span_buf(std::ios_base::openmode which)
        : my_base(), m_mode(which) {}

    explicit span_buf(
        std::span<char> sp,
        std::ios_base::openmode which = std::ios_base::in | std::ios_base::out
    )
        : my_base(), m_mode(which), m_buf()
    {
        this->span(sp);
    }

    span_buf(const span_buf&) = delete;

    span_buf(span_buf&& other)
        : my_base(std::move(other)),
          m_mode(other.m_mode),
          m_buf(std::exchange(other.m_buf, std::span<char>()))
    {
        other.setp(nullptr, nullptr);
        other.setg(nullptr, nullptr, nullptr);
    }

    span_buf& operator=(const span_buf&) = delete;

    span_buf& operator=(span_buf&& rhs) noexcept
    {
        if(this == &rhs)
            return *this;

        m_buf = std::span<char>();
        this->setp(nullptr, nullptr);
        this->setg(nullptr, nullptr, nullptr);
        this->swap(rhs);

        return *this;
    }

    void swap(span_buf& other) noexcept
    {
        my_base::swap(other);
        std::swap(m_mode, other.m_mode);
        std::swap(m_buf, other.m_buf);
    }

    [[nodiscard]]
    std::span<char> span() const noexcept
    {
        if(m_mode & std::ios_base::out)
        {
            return std::span<char>(my_base::pbase(), my_base::pptr());
        }

        return m_buf;
    }

    void span(std::span<char> sp) noexcept
    {
        m_buf = sp;
        char* start = m_buf.data();
        char* stop = start + m_buf.size();
        if(m_mode & std::ios_base::out)
        {
            if(m_mode & std::ios_base::ate)
            {
                my_base::setp(stop, stop);
            }
            else
            {
                my_base::setp(start, stop);
            }
        }

        if(m_mode & std::ios_base::in)
        {
            my_base::setg(start, start, stop);
        }
    }

private:
    pos_type seekoff(
        off_type off, std::ios_base::seekdir way, std::ios_base::openmode which
    ) override
    {
        const bool is_in = which & std::ios_base::in;
        const bool is_out = which & std::ios_base::out;
        switch(way)
        {
        case std::ios_base::beg:
            if(static_cast<std::size_t>(off) > m_buf.size())
            {
                return pos_type(off_type(-1));
            }
            break;

        case std::ios_base::end:
            {
                const auto base_off =
                    (m_mode & std::ios_base::out) && !(m_mode & std::ios_base::in) ?
                        static_cast<off_type>(my_base::pptr() - my_base::pbase()) :
                        static_cast<off_type>(m_buf.size());
                if(off > std::numeric_limits<off_type>::max() - base_off)
                {
                    return pos_type(off_type(-1));
                }

                off += base_off;
                if(static_cast<size_t>(off) > m_buf.size())
                {
                    return pos_type(off_type(-1));
                }
            }
            break;

        case std::ios_base::cur:
            if(is_in && is_out)
            {
                return pos_type(off_type(-1)); // report failure
            }
            else if(is_in || is_out)
            {
                const off_type old_off = is_in ? static_cast<off_type>(my_base::gptr() - my_base::eback()) : static_cast<off_type>(my_base::pptr() - my_base::pbase());
                const off_type old_left = static_cast<off_type>(m_buf.size() - old_off);
                if(off < -old_off || off > old_left)
                { // out of range
                    return pos_type(off_type(-1));
                }

                off += old_off;
            }
            else
            {
                return pos_type(off_type(-1)); // report failure
            }
            break;

        default:
            return pos_type(off_type(-1)); // report failure
        }

        if(off != 0 &&
           ((is_in && !my_base::gptr()) || (is_out && !my_base::pptr())))
        {
            return pos_type(off_type(-1)); // report failure
        }

        if(is_in)
        {
            my_base::gbump(
                static_cast<int>(off - (my_base::gptr() - my_base::eback()))
            );
        }

        if(is_out)
        {
            my_base::pbump(
                static_cast<int>(off - (my_base::pptr() - my_base::pbase()))
            );
        }

        return pos_type{off};
    }

    pos_type seekpos(pos_type pos, std::ios_base::openmode which) override
    {
        return seekoff(off_type(pos), std::ios_base::beg, which);
    }

    span_buf* setbuf(char* buf, std::streamsize count) override
    {
        this->span(std::span<char>(buf, static_cast<std::size_t>(count)));
        return this;
    }

    std::ios_base::openmode m_mode =
        std::ios_base::in | std::ios_base::out;
    std::span<char> m_buf;
};
} // namespace lochfolk

#endif
