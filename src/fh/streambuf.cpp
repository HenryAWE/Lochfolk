#include "streambuf.hpp"
#include <algorithm>
#include <ios>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>

namespace lochfolk
{
fh_streambuf::fh_streambuf(std::unique_ptr<file_handle> fh)
    : m_fh(std::move(fh))
{
    if(!m_fh)
        throw std::invalid_argument("lochfolk: stream buffer needs a handle");

    setg(m_buf, m_buf, m_buf);
}

bool fh_streambuf::fill()
{
    if(m_eof)
        return false;

    const io_result result = m_fh->read(
        std::span<std::byte>(
            reinterpret_cast<std::byte*>(m_buf), buf_size
        )
    );

    if(result.is_npos())
        throw std::ios_base::failure("lochfolk: read failed");

    const auto size = static_cast<std::size_t>(result.bytes_transferred());
    if(size == 0)
    {
        m_eof = true;
        return false;
    }

    m_fill_end += size;
    setg(m_buf, m_buf, m_buf + size);
    return true;
}

std::uint64_t fh_streambuf::logical_pos() const
{
    const auto buffered = static_cast<std::uint64_t>(egptr() - gptr());
    return m_fill_end - buffered;
}

fh_streambuf::int_type fh_streambuf::underflow()
{
    if(gptr() < egptr())
        return traits_type::to_int_type(*gptr());

    if(!fill())
        return traits_type::eof();

    return traits_type::to_int_type(*gptr());
}

fh_streambuf::int_type fh_streambuf::pbackfail(int_type ch)
{
    if(gptr() <= eback())
        return traits_type::eof();

    // The stream is input only, so a different character cannot be accepted.
    if(!traits_type::eq_int_type(ch, traits_type::eof()) &&
       !traits_type::eq(traits_type::to_char_type(ch), gptr()[-1]))
    {
        return traits_type::eof();
    }

    gbump(-1);
    return traits_type::not_eof(ch);
}

std::streamsize fh_streambuf::xsgetn(char_type* s, std::streamsize count)
{
    std::streamsize total = 0;
    while(total < count)
    {
        if(gptr() == egptr())
        {
            if(!fill())
                break;
        }

        const auto available = static_cast<std::streamsize>(egptr() - gptr());
        const std::streamsize n = std::min(available, count - total);
        traits_type::copy(s + total, gptr(), static_cast<std::size_t>(n));
        gbump(static_cast<int>(n));
        total += n;
    }

    return total;
}

std::streamsize fh_streambuf::showmanyc()
{
    return static_cast<std::streamsize>(egptr() - gptr());
}

fh_streambuf::pos_type fh_streambuf::seekoff(
    off_type off,
    std::ios_base::seekdir way,
    std::ios_base::openmode which
)
{
    // The stream is input only: a request that also asks for output cannot
    // be satisfied.
    if(!(which & std::ios_base::in) || (which & std::ios_base::out))
        return pos_type(off_type(-1));

    std::int64_t base = 0;
    switch(way)
    {
    case std::ios_base::beg:
        base = 0;
        break;

    case std::ios_base::cur:
        base = static_cast<std::int64_t>(logical_pos());
        break;

    case std::ios_base::end:
        base = static_cast<std::int64_t>(m_fh->file_size());
        break;

    default:
        return pos_type(off_type(-1));
    }

    std::int64_t target = 0;
    if(off < 0)
    {
        // Compute the magnitude without negating the most negative value.
        const auto delta = static_cast<std::uint64_t>(-(off + 1)) + 1;
        if(delta > static_cast<std::uint64_t>(base))
            return pos_type(off_type(-1));

        target = base - static_cast<std::int64_t>(delta);
    }
    else
    {
        const auto delta = static_cast<std::uint64_t>(off);
        const auto room = static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max() - base
        );
        if(delta > room)
            return pos_type(off_type(-1));

        target = base + static_cast<std::int64_t>(delta);
    }

    if(!m_fh->seek(target, whence::begin))
        return pos_type(off_type(-1));

    m_fill_end = static_cast<std::uint64_t>(target);
    m_eof = false;
    setg(m_buf, m_buf, m_buf);
    return pos_type(off_type(target));
}

fh_streambuf::pos_type fh_streambuf::seekpos(
    pos_type pos,
    std::ios_base::openmode which
)
{
    return seekoff(off_type(pos), std::ios_base::beg, which);
}
} // namespace lochfolk
