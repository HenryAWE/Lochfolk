#include <lochfolk/vfs.hpp>
#include <memory>
#include <cassert>
#include <lochfolk/utility.hpp>
#include "errmsg.hpp"
#include "file_node.hpp"

namespace lochfolk
{
struct virtual_file_system::vfs_data
{
    detail::file_node root;

    vfs_data()
        : root(nullptr, std::in_place_type<file_data::directory>) {}
};

virtual_file_system::virtual_file_system()
    : m_vfs_data(new vfs_data())
{
    assert(m_vfs_data->root.is_directory());
}

virtual_file_system::~virtual_file_system()
{
    delete m_vfs_data;
}

void virtual_file_system::mount_string_constant(
    path_view p, std::string_view str, bool overwrite
)
{
    mount_impl(
        m_vfs_data->root,
        p,
        overwrite,
        std::in_place_type<file_data::string_constant>,
        str
    );
}

void virtual_file_system::mount_string_constant(
    path_view p, const char* str, bool overwrite
)
{
    mount_string_constant(
        p,
        std::string_view(str),
        overwrite
    );
}

void virtual_file_system::mount_string_constant(
    path_view p, std::string str, bool overwrite
)
{
    mount_impl(
        m_vfs_data->root,
        p,
        overwrite,
        std::in_place_type<file_data::string_constant>,
        std::move(str)
    );
}

void virtual_file_system::mount_sys_file(
    path_view p, const std::filesystem::path& sys_path, bool overwrite
)
{
    namespace stdfs = std::filesystem;

    if(!stdfs::exists(sys_path))
    {
        throw error(stdfs_err_msg(sys_path, " does not exist"));
    }
    else if(stdfs::is_regular_file(sys_path))
    {
        mount_impl(
            m_vfs_data->root,
            p,
            overwrite,
            std::in_place_type<file_data::sys_file>,
            stdfs::absolute(sys_path)
        );
    }
    else
    {
        throw error(stdfs_err_msg({}, sys_path, " is not a regular file"));
    }
}

void virtual_file_system::mount_sys_dir(
    path_view p, const std::filesystem::path& dir, bool overwrite
)
{
    namespace stdfs = std::filesystem;

    if(!stdfs::is_directory(dir))
    {
        throw error(stdfs_err_msg(dir, " is not a directory"));
    }

    path base(p);
    for(auto& i : stdfs::recursive_directory_iterator(dir))
    {
        if(stdfs::is_directory(i))
            continue;

        stdfs::path file_path = stdfs::absolute(i);
        std::u8string filename = stdfs::relative(file_path, dir).generic_u8string();
        mount_impl(
            m_vfs_data->root,
            base / std::string_view((const char*)filename.c_str(), filename.size()),
            overwrite,
            std::in_place_type<file_data::sys_file>,
            file_path
        );
    }
}

void virtual_file_system::mount_zip_archive(
    path_view p, const std::filesystem::path& sys_path, bool overwrite
)
{
    std::shared_ptr ar = std::make_shared<zip_archive>();
    ar->open(sys_path);

    path base(p);

    if(!ar->goto_first())
        return; // Empty archive
    do
    {
        if(ar->current_is_dir())
            continue;

        auto entry = ar->open_current();
        std::string_view filename = entry.filename();

        mount_impl(
            m_vfs_data->root,
            base / filename,
            overwrite,
            std::in_place_type<file_data::archive_entry>,
            *ar,
            entry.offset()
        );
    } while(ar->goto_next());
}

bool virtual_file_system::exists(path_view p) const
{
    return find_impl(m_vfs_data->root, p) != nullptr;
}

bool virtual_file_system::is_directory(path_view p) const
{
    const auto* f = find_impl(m_vfs_data->root, p);
    if(!f)
        return false;
    return f->is_directory();
}

std::uint64_t virtual_file_system::file_size(path_view p) const
{
    const auto* f = find_impl(m_vfs_data->root, p);
    if(!f) [[unlikely]]
        throw error(vfs_err_msg(p, " is not found"));

    return f->file_size();
}

bool virtual_file_system::remove(path_view p)
{
    if(p.empty() || !p.is_absolute()) [[unlikely]]
        return false;
    if(p == "/"_pv)
    {
        auto* root_dir = m_vfs_data->root.get_if<file_data::directory>();
        assert(root_dir);

        root_dir->children().clear();
        return true;
    }

    auto* parent = find_impl(m_vfs_data->root, p.parent_path());
    if(!parent->is_directory())
        return false;

    std::string_view target_sv = [](path_view pv)
    {
        std::string_view result(pv);
        if(result.back() == path_view::separator)
            result.remove_suffix(1);

        std::size_t pos = result.rfind(path_view::separator);
        if(pos == std::string_view::npos)
            pos = 0;
        return result.substr(pos + 1);
    }(p);

    auto* parent_dir = parent->get_if<file_data::directory>();
    auto it = parent_dir->children().find(path_view(target_sv));
    if(it == parent_dir->children().end())
        return false;

    parent_dir->children().erase(it);
    return true;
}

ivfstream virtual_file_system::open(path_view p, std::ios_base::openmode mode)
{
    const auto* f = find_impl(m_vfs_data->root, p);
    if(!f)
        throw error(vfs_err_msg(p, " is not found"));

    mode |= std::ios_base::in;
    return ivfstream(f->getbuf(mode));
}

std::string virtual_file_system::read_string(path_view p, bool convert_crlf)
{
    const auto* f = find_impl(m_vfs_data->root, p);
    if(!f)
        throw error(vfs_err_msg(p, " is not found"));

    return f->read_string(convert_crlf);
}

void virtual_file_system::list_files(std::ostream& os)
{
    list_files_impl(os, "/", m_vfs_data->root, 0);
}

access_context::access_context(access_context&& other) noexcept
    : m_vfs(other.m_vfs), m_current(std::move(other.m_current)) {}

access_context::access_context(const access_context&) = default;

access_context::access_context(virtual_file_system& vfs)
    : m_vfs(&vfs), m_current("/") {}

void access_context::current_path(path_view pv)
{
    m_current /= pv;
    m_current = m_current.lexically_normal();
}

path access_context::to_fullpath(path_view pv) const
{
    path result = m_current / pv;

    return result.lexically_normal();
}
} // namespace lochfolk
