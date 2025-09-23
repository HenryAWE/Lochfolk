#include "file_node.hpp"
#include <memory>
#include <fstream>
#include <sstream>
#include <lochfolk/vfs.hpp>
#include <lochfolk/utility.hpp>
#include "errmsg.hpp"

namespace lochfolk
{
std::uint64_t file_node::directory::file_size() const noexcept
{
    return 0;
}

std::unique_ptr<std::streambuf> file_node::string_constant::open(
    std::ios_base::openmode mode
) const
{
    mode &= ~std::ios_base::out;

    return std::visit(
        [mode]<typename T>(const T& v) -> std::unique_ptr<std::streambuf>
        {
            if constexpr(std::same_as<std::remove_cvref_t<T>, std::string_view>)
            {
                std::span<char> sp(const_cast<char*>(v.data()), v.size());
                return std::make_unique<span_buf>(sp, mode);
            }
            else // std::string
            {
                return std::make_unique<std::stringbuf>(v, mode);
            }
        },
        m_str_data
    );
}

std::string file_node::string_constant::read_string(bool convert_crlf) const
{
    (void)convert_crlf;
    return std::string(view());
}

std::uint64_t file_node::string_constant::file_size() const
{
    return std::visit(
        [](const auto& v) -> std::uint64_t
        { return static_cast<std::uint64_t>(v.size()); },
        m_str_data
    );
}

std::string_view file_node::string_constant::view() const noexcept
{
    return std::visit(
        [](const auto& v) -> std::string_view
        { return v; },
        m_str_data
    );
}

std::unique_ptr<std::filebuf> file_node::sys_file::open(
    std::ios_base::openmode mode
) const
{
    std::unique_ptr fb = std::make_unique<std::filebuf>();
    fb->open(m_sys_path, mode);
    if(!fb->is_open())
        throw virtual_file_system::error(stdfs_err_msg("failed to open ", m_sys_path));

    return fb;
}

std::string file_node::sys_file::read_string(bool convert_crlf) const
{
    std::ios_base::openmode mode = std::ios_base::in;
    if(!convert_crlf)
        mode |= std::ios_base::binary;

    std::ifstream ifs(m_sys_path, mode);
    if(!ifs.is_open()) [[unlikely]]
        throw virtual_file_system::error(stdfs_err_msg("failed to open ", m_sys_path));

    std::stringstream ss;
    ss << ifs.rdbuf();
    return std::move(ss).str();
}

std::uint64_t file_node::sys_file::file_size() const
{
    return static_cast<std::uint64_t>(
        std::filesystem::file_size(m_sys_path)
    );
}

file_node::archive_entry::archive_entry(archive& ar, std::int64_t off)
    : m_archive_ref(ar.shared_from_this()), m_offset(off)
{}

file_node::archive_entry& file_node::archive_entry::operator=(
    archive_entry&& rhs
) noexcept
{
    if(this == &rhs) [[unlikely]]
        return *this;

    m_archive_ref = std::move(rhs.m_archive_ref);
    m_offset = std::exchange(rhs.m_offset, 0);

    return *this;
}

std::unique_ptr<std::streambuf> file_node::archive_entry::open(
    std::ios_base::openmode mode
) const
{
    return m_archive_ref->getbuf(m_offset, mode);
}

std::string file_node::archive_entry::read_string(bool convert_crlf) const
{
    (void)convert_crlf;
    return m_archive_ref->read_string(m_offset);
}

std::uint64_t file_node::archive_entry::file_size() const
{
    return m_archive_ref->get_file_size(m_offset);
}

bool file_node::is_directory() const noexcept
{
    return std::holds_alternative<directory>(m_data);
}

std::uint64_t file_node::file_size() const
{
    return std::visit(
        [](const auto& v) -> std::uint64_t
        {
            return v.file_size();
        },
        m_data
    );
}

std::unique_ptr<std::streambuf> file_node::getbuf(
    std::ios_base::openmode mode
) const
{
    return visit(
        [mode]<typename T>(const T& v) -> std::unique_ptr<std::streambuf>
        {
            constexpr bool has_buf = requires() { v.open(mode); };
            if constexpr(has_buf)
                return v.open(mode);
            throw virtual_file_system::error("bad file");
        }
    );
}

std::string file_node::read_string(bool convert_crlf) const
{
    return visit(
        [convert_crlf]<typename T>(const T& v) -> std::string
        {
            constexpr bool has_read_string = requires() { v.read_string(convert_crlf); };
            if constexpr(has_read_string)
                return v.read_string(convert_crlf);
            throw virtual_file_system::error("bad file");
        }
    );
}
} // namespace lochfolk
