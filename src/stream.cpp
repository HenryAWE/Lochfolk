#include <lochfolk/stream.hpp>
#include "fh/streambuf.hpp"

namespace lochfolk
{
ivfstream::ivfstream(ivfstream&& other) noexcept
    : my_base(other.rdbuf(nullptr)),
      m_buf(std::move(other.m_buf)) {}

ivfstream::ivfstream(std::unique_ptr<std::streambuf> buf)
    : my_base(buf.get()),
      m_buf(std::move(buf)) {}

ivfstream::ivfstream(std::unique_ptr<file_handle> fh)
    : my_base(nullptr),
      m_buf(std::make_unique<fh_streambuf>(std::move(fh)))
{
    my_base::rdbuf(m_buf.get());
}

ivfstream::~ivfstream() = default;

bool ivfstream::has_buffer() const noexcept
{
    return static_cast<bool>(m_buf);
}
} // namespace lochfolk
