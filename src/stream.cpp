#include <lochfolk/stream.hpp>

namespace lochfolk
{
ivfstream::ivfstream(ivfstream&& other) noexcept
    : my_base(other.rdbuf(nullptr)),
      m_buf(std::move(other.m_buf)) {}

ivfstream::ivfstream(std::unique_ptr<std::streambuf> buf)
    : my_base(buf.get()),
      m_buf(std::move(buf)) {}

ivfstream::~ivfstream() = default;

bool ivfstream::has_buffer() const noexcept
{
    return static_cast<bool>(m_buf);
}
} // namespace lochfolk
