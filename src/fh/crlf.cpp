#include "crlf.hpp"
#include "crlf_convert.hpp"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace lochfolk
{

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
    std::array<std::byte, chunk_size> raw;
    const io_result result = m_inner->read(std::span(raw));
    if(result.is_npos())
        return false;

    const auto size = static_cast<std::size_t>(result.bytes_transferred());
    m_begin = 0;

    if(size == 0)
    {
        m_eof = true;
        // An empty chunk flushes a CR that was held back at end of file.
        m_end = detail::convert_crlf(
            std::span<const std::byte>(),
            std::span<std::byte>(m_buf),
            m_pending_cr
        );

        return true;
    }

    m_end = detail::convert_crlf(
        std::span<const std::byte>(raw.data(), size),
        std::span<std::byte>(m_buf),
        m_pending_cr
    );

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

            assert(m_end <= m_buf.size());

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
