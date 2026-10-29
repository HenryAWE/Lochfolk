#include "crlf.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace lochfolk
{
namespace
{
    constexpr std::byte cr = static_cast<std::byte>('\r');
    constexpr std::byte lf = static_cast<std::byte>('\n');
} // namespace

fh_crlf::fh_crlf(std::unique_ptr<file_handle> inner)
    : m_inner(std::move(inner))
{
    if(!m_inner)
        throw std::invalid_argument("lochfolk: CRLF filter needs a handle");
}

file_flag fh_crlf::get_flags() const
{
    return m_inner->get_flags();
}

std::uint64_t fh_crlf::file_size() const
{
    return m_inner->file_size();
}

bool fh_crlf::seek(std::int64_t off, whence w)
{
    if(!m_inner->seek(off, w))
        return false;

    m_begin = 0;
    m_end = 0;
    m_pending_cr = false;
    m_eof = false;
    return true;
}

bool fh_crlf::refill()
{
    std::array<std::byte, 4096> raw;
    const io_result result = m_inner->read(std::span(raw));
    if(result.is_npos())
        return false;

    const auto size = static_cast<std::size_t>(result.bytes_transferred());
    m_begin = 0;
    m_end = 0;

    if(size == 0)
    {
        m_eof = true;
        if(m_pending_cr)
        {
            // The file ends with a lone CR that we held back.
            m_buf[0] = cr;
            m_end = 1;
            m_pending_cr = false;
        }

        return true;
    }

    std::size_t out = 0;
    std::size_t i = 0;

    if(m_pending_cr)
    {
        m_pending_cr = false;
        if(raw[0] == lf)
        {
            // The LF completes a CRLF pair
            m_buf[out++] = lf;
            i = 1;
        }
        else
        {
            m_buf[out++] = cr; // The CR was not followed by an LF
        }
    }

    for(; i < size; ++i)
    {
        if(raw[i] != cr)
        {
            m_buf[out++] = raw[i];
            continue;
        }

        if(i + 1 < size)
        {
            if(raw[i + 1] == lf)
            {
                m_buf[out++] = lf;
                ++i;
            }
            else
            {
                m_buf[out++] = cr;
            }
        }
        else
        {
            // The next byte is in the following chunk.
            m_pending_cr = true;
        }
    }

    m_end = out;
    return true;
}

io_result fh_crlf::read(std::span<std::byte> buf)
{
    if(buf.empty())
        return io_result(0);

    std::size_t produced = 0;
    while(produced < buf.size())
    {
        if(m_begin == m_end)
        {
            if(m_eof)
                break;

            if(!refill())
                return io_result();

            if(m_begin == m_end)
            {
                if(m_eof)
                    break; // End of file

                // The chunk held nothing but a CR that may start a CRLF
                // pair in the next chunk; keep reading instead of
                // reporting a premature end of file.
                continue;
            }
        }

        const auto count =
            std::min(m_end - m_begin, buf.size() - produced);
        std::memcpy(buf.data() + produced, m_buf.data() + m_begin, count);
        m_begin += count;
        produced += count;
    }

    return io_result(static_cast<std::uint64_t>(produced));
}

io_result fh_crlf::write(std::span<const std::byte> buf)
{
    return m_inner->write(buf);
}
} // namespace lochfolk
