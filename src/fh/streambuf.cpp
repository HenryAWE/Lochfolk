#include "streambuf.hpp"
#include <algorithm>

namespace lochfolk
{
fh_streambuf::fh_streambuf(std::unique_ptr<file_handle> fh)
    : my_base(), m_fh(std::move(fh)), m_pos(0)
{
    setg(m_buf + putback_size, m_buf + putback_size, m_buf + putback_size);
}

fh_streambuf::int_type fh_streambuf::underflow()
{
    if(gptr() < egptr())
        return traits_type::to_int_type(*gptr());

    auto result = m_fh->read(
        std::span(reinterpret_cast<std::byte*>(m_buf + putback_size), buf_size),
        buf_size
    );
    if(result.is_npos())
        return traits_type::eof();

    auto nread = static_cast<std::streamsize>(static_cast<std::uint64_t>(result));
    if(nread == 0)
        return traits_type::eof();

    m_pos += static_cast<std::uint64_t>(nread);
    setg(
        m_buf,
        m_buf + putback_size,
        m_buf + putback_size + nread
    );
    return traits_type::to_int_type(*gptr());
}

std::streamsize fh_streambuf::xsgetn(char_type* s, std::streamsize count)
{
    std::streamsize total = 0;

    // Drain buffered data first
    std::streamsize buffered = egptr() - gptr();
    if(buffered > 0)
    {
        std::streamsize n = std::min(buffered, count);
        traits_type::copy(s, gptr(), n);
        gbump(static_cast<int>(n));
        s += n;
        count -= n;
        total += n;
    }

    // Direct read from file_handle for remaining bytes
    if(count > 0)
    {
        auto result = m_fh->read(
            std::span(reinterpret_cast<std::byte*>(s), static_cast<std::size_t>(count)),
            static_cast<std::uint64_t>(count)
        );
        if(!result.is_npos())
        {
            auto n = static_cast<std::streamsize>(static_cast<std::uint64_t>(result));
            if(n > 0)
            {
                m_pos += static_cast<std::uint64_t>(n);
                total += n;
            }
        }
        setg(m_buf + putback_size, m_buf + putback_size, m_buf + putback_size);
    }

    return total;
}

fh_streambuf::pos_type fh_streambuf::seekoff(
    off_type off,
    std::ios_base::seekdir way,
    std::ios_base::openmode which
)
{
    if(!(which & std::ios_base::in))
        return pos_type(off_type(-1));

    std::uint64_t new_pos;

    switch(way)
    {
    case std::ios_base::beg:
        if(off < 0)
            return pos_type(off_type(-1));
        new_pos = static_cast<std::uint64_t>(off);
        break;

    case std::ios_base::cur:
        {
            // Logical position = end-of-buffer pos minus unconsumed bytes
            std::uint64_t logical = m_pos - static_cast<std::uint64_t>(egptr() - gptr());
            if(off < 0 && static_cast<std::uint64_t>(-off) > logical)
                return pos_type(off_type(-1));
            if(off > 0 && static_cast<std::uint64_t>(off) > std::numeric_limits<std::uint64_t>::max() - logical)
                return pos_type(off_type(-1));
            new_pos = static_cast<std::uint64_t>(static_cast<std::int64_t>(logical) + off);
        }
        break;

    case std::ios_base::end:
        {
            std::uint64_t fsz = m_fh->file_size();
            if(off < 0 && static_cast<std::uint64_t>(-off) > fsz)
                return pos_type(off_type(-1));
            if(off > 0 && static_cast<std::uint64_t>(off) > std::numeric_limits<std::uint64_t>::max() - fsz)
                return pos_type(off_type(-1));
            new_pos = static_cast<std::uint64_t>(static_cast<std::int64_t>(fsz) + off);
        }
        break;

    default:
        return pos_type(off_type(-1));
    }

    if(!m_fh->seek(static_cast<std::int64_t>(new_pos), whence::set))
        return pos_type(off_type(-1));

    m_pos = new_pos;
    setg(m_buf + putback_size, m_buf + putback_size, m_buf + putback_size);
    return pos_type(static_cast<off_type>(new_pos));
}

fh_streambuf::pos_type fh_streambuf::seekpos(
    pos_type pos,
    std::ios_base::openmode which
)
{
    return seekoff(off_type(pos), std::ios_base::beg, which);
}
} // namespace lochfolk
