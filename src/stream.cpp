#include <lochfolk/stream.hpp>

namespace lochfolk
{
ivfstream::ivfstream(ivfstream&& other) noexcept
    : my_base(other.rdbuf(nullptr)) {}

ivfstream::ivfstream(std::unique_ptr<std::streambuf> buf)
    : my_base(buf.release()) {}

ivfstream::~ivfstream()
{
    std::streambuf* buf = rdbuf(nullptr);
    delete buf;
}
} // namespace lochfolk
