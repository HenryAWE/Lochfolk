#include "posix.hpp"
#ifdef LOCHFOLK_IMPL_POSIX
#    include <fcntl.h>

namespace lochfolk
{
fh_posix_file::fh_posix_file(const std::filesystem::path& p)
    : m_fd(0)
{
    this->open(p);
}

fh_posix_file::~fh_posix_file()
{
    close();
}

bool fh_posix_file::is_open() const noexcept
{
    return m_fd != 0;
}

void fh_posix_file::open(const std::filesystem::path& p)
{
    this->close();
    m_fd = ::open(p.c_str(), O_RDONLY);
}

void fh_posix_file::close()
{
    if(!m_fd)
        return;
    ::close(m_fd);
}
} // namespace lochfolk

#endif
