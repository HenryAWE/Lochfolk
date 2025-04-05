#include <lochfolk/archive.hpp>

namespace lochfolk
{
archive::archive() = default;

archive::~archive() = default;

ivfstream::ivfstream(std::unique_ptr<std::streambuf> buf)
    : my_base(buf.release()) {}

ivfstream::~ivfstream()
{
    std::streambuf* buf = rdbuf(nullptr);
    delete buf;
}
} // namespace lochfolk
