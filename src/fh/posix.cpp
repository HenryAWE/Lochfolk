#include "posix.hpp"
#ifdef LOCHFOLK_IMPL_POSIX
#    include <fcntl.h>
#    include <sys/stat.h>
#    include <unistd.h>

namespace lochfolk
{
fh_posix_file::fh_posix_file(const std::filesystem::path& p)
    : m_fd(-1), m_pos(0)
{
    this->open(p);
}

fh_posix_file::~fh_posix_file()
{
    close();
}

bool fh_posix_file::is_open() const noexcept
{
    return m_fd != -1;
}

void fh_posix_file::open(const std::filesystem::path& p)
{
    this->close();
    m_fd = ::open(p.c_str(), O_RDONLY);
    m_pos = 0;
}

void fh_posix_file::close()
{
    if(m_fd == -1)
        return;
    ::close(m_fd);
    m_fd = -1;
}

bool fh_posix_file::valid() const noexcept
{
    return is_open();
}

file_flag fh_posix_file::get_flags() const
{
    return file_flag::readable;
}

std::uint64_t fh_posix_file::file_size() const
{
    struct stat st;
    if(::fstat(m_fd, &st) != 0)
        return 0;
    return static_cast<std::uint64_t>(st.st_size);
}

bool fh_posix_file::seek(std::int64_t off, whence w)
{
    int posix_whence;
    switch(w)
    {
    case whence::set:
        posix_whence = SEEK_SET;
        break;
    case whence::cur:
        posix_whence = SEEK_CUR;
        break;
    case whence::end:
        posix_whence = SEEK_END;
        break;
    default:
        return false;
    }

    off_t result = ::lseek(m_fd, off, posix_whence);
    if(result == (off_t)-1)
        return false;
    m_pos = static_cast<std::uint64_t>(result);
    return true;
}

io_result fh_posix_file::read(std::span<std::byte> buf, std::uint64_t size)
{
    auto actual = std::min(buf.size_bytes(), static_cast<std::size_t>(size));
    ssize_t nread = ::read(m_fd, buf.data(), actual);
    if(nread < 0)
        return io_result();
    m_pos += static_cast<std::size_t>(nread);
    return io_result(static_cast<std::uint64_t>(nread));
}

io_result fh_posix_file::write(std::span<const std::byte> buf)
{
    (void)buf;
    return io_result(0);
}
} // namespace lochfolk

#endif
