#include "file.hpp"
#if defined(LOCHFOLK_FH_STDIO)

#    include <limits>
#    include <stdexcept>
#    include <system_error>
#    include <cerrno>

namespace lochfolk
{
namespace
{
    [[noreturn]] void throw_errno(const char* what)
    {
        throw std::system_error(errno, std::generic_category(), what);
    }
} // namespace

fh_file::fh_file(const std::filesystem::path& p, file_flag flags)
    : m_flags(flags)
{
    if(!has_flag(flags, file_flag::readable) &&
       !has_flag(flags, file_flag::writable))
    {
        throw std::invalid_argument("lochfolk: no access mode requested");
    }

    // No "w" mode: the file must exist and is never truncated, which
    // matches the behaviour of the POSIX and Win32 backends.
    const char* mode = nullptr;
    if(has_flag(flags, file_flag::writable))
        mode = "r+b"; // stdio has no write-only update mode
    else
        mode = "rb";

    m_file = std::fopen(p.string().c_str(), mode);
    if(m_file == nullptr)
        throw_errno("lochfolk: cannot open file");
}

fh_file::~fh_file()
{
    if(m_file != nullptr)
        std::fclose(m_file);
}

std::uint64_t fh_file::file_size() const
{
    const long pos = std::ftell(m_file);
    if(pos < 0 || std::fseek(m_file, 0, SEEK_END) != 0)
        throw_errno("lochfolk: cannot query file size");

    const long end = std::ftell(m_file);
    const int restore = std::fseek(m_file, pos, SEEK_SET);
    if(end < 0 || restore != 0)
        throw_errno("lochfolk: cannot query file size");

    return static_cast<std::uint64_t>(end);
}

bool fh_file::seek(std::int64_t off, whence w)
{
    int dir = 0;
    switch(w)
    {
    case whence::begin:
        dir = SEEK_SET;
        break;

    case whence::current:
        dir = SEEK_CUR;
        break;

    case whence::end:
        dir = SEEK_END;
        break;
    }

    // stdio only guarantees long offsets; this backend exists for platforms
    // without a native implementation, so refuse offsets that do not fit.
    if(off < std::numeric_limits<long>::min() ||
       off > std::numeric_limits<long>::max())
    {
        return false;
    }

    return std::fseek(m_file, static_cast<long>(off), dir) == 0;
}

io_result fh_file::read(std::span<std::byte> buf)
{
    if(!readable())
        return io_result();

    if(buf.empty())
        return io_result(0);

    const std::size_t n = std::fread(buf.data(), 1, buf.size(), m_file);
    if(n == 0 && std::ferror(m_file) != 0)
        return io_result();

    return io_result(static_cast<std::uint64_t>(n));
}

io_result fh_file::write(std::span<const std::byte> buf)
{
    if(!writable())
        return io_result();

    if(buf.empty())
        return io_result(0);

    const std::size_t n = std::fwrite(buf.data(), 1, buf.size(), m_file);
    if(n != buf.size() || std::fflush(m_file) != 0)
        return io_result();

    return io_result(static_cast<std::uint64_t>(n));
}
} // namespace lochfolk

#endif
