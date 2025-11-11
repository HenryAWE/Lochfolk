#include <lochfolk/io.hpp>

namespace lochfolk
{
file_handle::file_handle() = default;

file_handle::~file_handle() = default;

bool file_handle::valid() const noexcept
{
    // Placeholder for empty file handle
    return false;
}

std::uint64_t file_handle::file_size() const
{
    // Placeholder
    return 0;
}

bool file_handle::seek(std::int64_t off, whence w)
{
    (void)off;
    (void)w;
    return false;
}
} // namespace lochfolk
