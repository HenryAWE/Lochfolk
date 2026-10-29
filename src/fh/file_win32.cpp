// These must be defined before the first inclusion of <windows.h>
#ifndef NOMINMAX
#    define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#endif

#include "file.hpp"
#if defined(LOCHFOLK_FH_WIN32)

#    include <windows.h>

#    include <algorithm>
#    include <limits>
#    include <stdexcept>
#    include <string>
#    include <system_error>

namespace lochfolk
{
namespace
{
    [[noreturn]] void throw_last_error(const char* what)
    {
        throw std::system_error(
            static_cast<int>(::GetLastError()),
            std::system_category(),
            what
        );
    }

    HANDLE to_handle(void* handle) noexcept
    {
        return static_cast<HANDLE>(handle);
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

    DWORD access = 0;
    if(has_flag(flags, file_flag::readable))
        access |= GENERIC_READ;
    if(has_flag(flags, file_flag::writable))
        access |= GENERIC_WRITE;

    const HANDLE handle = ::CreateFileW(
        p.c_str(),
        access,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if(handle == INVALID_HANDLE_VALUE)
        throw_last_error("lochfolk: cannot open file");

    m_handle = handle;
}

fh_file::~fh_file()
{
    if(m_handle != nullptr)
        ::CloseHandle(to_handle(m_handle));
}

std::uint64_t fh_file::file_size() const
{
    LARGE_INTEGER size;
    if(!::GetFileSizeEx(to_handle(m_handle), &size))
        throw_last_error("lochfolk: cannot query file size");

    return static_cast<std::uint64_t>(size.QuadPart);
}

bool fh_file::seek(std::int64_t off, whence w)
{
    DWORD method = 0;
    switch(w)
    {
    case whence::begin:
        method = FILE_BEGIN;
        break;

    case whence::current:
        method = FILE_CURRENT;
        break;

    case whence::end:
        method = FILE_END;
        break;
    }

    LARGE_INTEGER distance;
    distance.QuadPart = off;

    LARGE_INTEGER new_pos;
    return ::SetFilePointerEx(
               to_handle(m_handle), distance, &new_pos, method
           ) != 0;
}

io_result fh_file::read(std::span<std::byte> buf)
{
    if(!readable())
        return io_result();

    std::size_t total = 0;
    while(total < buf.size())
    {
        const auto remaining = buf.size() - total;
        const auto chunk = static_cast<DWORD>(
            std::min<std::size_t>(
                remaining, static_cast<std::size_t>(MAXDWORD)
            )
        );

        DWORD nread = 0;
        if(!::ReadFile(
               to_handle(m_handle), buf.data() + total, chunk, &nread, nullptr
           ))
        {
            return io_result();
        }

        if(nread == 0)
            break;

        total += static_cast<std::size_t>(nread);
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
        const auto remaining = buf.size() - total;
        const auto chunk = static_cast<DWORD>(
            std::min<std::size_t>(
                remaining, static_cast<std::size_t>(MAXDWORD)
            )
        );

        DWORD nwritten = 0;
        if(!::WriteFile(
               to_handle(m_handle), buf.data() + total, chunk, &nwritten, nullptr
           ))
        {
            return io_result();
        }

        if(nwritten == 0)
            break;

        total += static_cast<std::size_t>(nwritten);
    }

    return io_result(static_cast<std::uint64_t>(total));
}
} // namespace lochfolk

#endif
