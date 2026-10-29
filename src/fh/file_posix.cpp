#include "file.hpp"
#if defined(LOCHFOLK_FH_POSIX)

#    include <cerrno>
#    include <stdexcept>
#    include <system_error>
#    include <fcntl.h>
#    include <sys/stat.h>
#    include <unistd.h>

namespace lochfolk
{
namespace
{
    [[noreturn]] void throw_system_error(int err, const char* what)
    {
        throw std::system_error(err, std::generic_category(), what);
    }
} // namespace

fh_file::fh_file(const std::filesystem::path& p, file_flag flags)
    : m_flags(flags)
{
    const bool can_read = has_flag(flags, file_flag::readable);
    const bool can_write = has_flag(flags, file_flag::writable);
    if(!can_read && !can_write)
        throw std::invalid_argument("lochfolk: no access mode requested");

    int oflags = 0;
    if(can_read && can_write)
        oflags = O_RDWR;
    else if(can_write)
        oflags = O_WRONLY;
    else
        oflags = O_RDONLY;

    int fd;
    do
    {
        fd = ::open(p.c_str(), oflags | O_CLOEXEC);
    } while(fd == -1 && errno == EINTR);

    if(fd == -1)
        throw_system_error(errno, "lochfolk: cannot open file");

    m_fd = fd;
}

fh_file::~fh_file()
{
    if(m_fd != -1)
        ::close(m_fd);
}

std::uint64_t fh_file::file_size() const
{
    struct stat st;
    if(::fstat(m_fd, &st) != 0)
        throw_system_error(errno, "lochfolk: cannot query file size");

    return static_cast<std::uint64_t>(st.st_size);
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

    off_t result;
    do
    {
        result = ::lseek(m_fd, static_cast<off_t>(off), dir);
    } while(result == static_cast<off_t>(-1) && errno == EINTR);

    return result != static_cast<off_t>(-1);
}

io_result fh_file::read(std::span<std::byte> buf)
{
    if(!readable())
        return io_result();

    std::size_t total = 0;
    while(total < buf.size())
    {
        const ssize_t n = ::read(m_fd, buf.data() + total, buf.size() - total);
        if(n < 0)
        {
            if(errno == EINTR)
                continue;

            return io_result();
        }

        if(n == 0)
            break;

        total += static_cast<std::size_t>(n);
    }

    return io_result(static_cast<std::uint64_t>(total));
}

io_result fh_file::write(std::span<const std::byte> buf)
{
    if(!writable())
        return io_result();

    std::size_t total = 0;
    while(total < buf.size())
    {
        const ssize_t n =
            ::write(m_fd, buf.data() + total, buf.size() - total);
        if(n < 0)
        {
            if(errno == EINTR)
                continue;

            return io_result();
        }

        if(n == 0)
            break;

        total += static_cast<std::size_t>(n);
    }

    return io_result(static_cast<std::uint64_t>(total));
}
} // namespace lochfolk

#endif
