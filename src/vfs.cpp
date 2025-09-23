#include <lochfolk/vfs.hpp>
#include <memory>
#include <cassert>
#include <lochfolk/utility.hpp>
#include "errmsg.hpp"
#include "file_node.hpp"

namespace lochfolk
{
virtual_file_system::virtual_file_system()
    : m_root(std::make_unique<file_node>(nullptr, std::in_place_type<file_node::directory>))
{
    assert(m_root->is_directory());
}

virtual_file_system::~virtual_file_system() = default;

void virtual_file_system::mount_string_constant(
    path_view p, std::string_view str, bool overwrite
)
{
    mount_impl(
        p,
        overwrite,
        std::in_place_type<file_node::string_constant>,
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
        p,
        overwrite,
        std::in_place_type<file_node::string_constant>,
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
            p,
            overwrite,
            std::in_place_type<file_node::sys_file>,
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
            base / std::string_view((const char*)filename.c_str(), filename.size()),
            overwrite,
            std::in_place_type<file_node::sys_file>,
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
            base / filename,
            overwrite,
            std::in_place_type<file_node::archive_entry>,
            *ar,
            entry.offset()
        );
    } while(ar->goto_next());
}

bool virtual_file_system::exists(path_view p) const
{
    return find_impl(p) != nullptr;
}

bool virtual_file_system::is_directory(path_view p) const
{
    const file_node* f = find_impl(p);
    if(!f)
        return false;
    return f->is_directory();
}

std::uint64_t virtual_file_system::file_size(path_view p) const
{
    const file_node* f = find_impl(p);
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
        auto* root_dir = m_root->get_if<file_node::directory>();
        assert(root_dir);

        root_dir->children().clear();
        return true;
    }

    auto* parent = find_impl(p.parent_path());
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

    auto* parent_dir = parent->get_if<file_node::directory>();
    auto it = parent_dir->children().find(path_view(target_sv));
    if(it == parent_dir->children().end())
        return false;

    parent_dir->children().erase(it);
    return true;
}

ivfstream virtual_file_system::open(path_view p, std::ios_base::openmode mode)
{
    const file_node* f = find_impl(p);
    if(!f)
        throw error(vfs_err_msg(p, " is not found"));

    mode |= std::ios_base::in;
    return ivfstream(f->getbuf(mode));
}

std::string virtual_file_system::read_string(path_view p, bool convert_crlf)
{
    const file_node* f = find_impl(p);
    if(!f)
        throw error(vfs_err_msg(p, " is not found"));

    return f->read_string(convert_crlf);
}

void virtual_file_system::list_files(std::ostream& os)
{
    list_files_impl(os, "/", *m_root, 0);
}

void virtual_file_system::list_files_impl(std::ostream& os, std::string_view name, const file_node& f, unsigned int indent)
{
    for(unsigned int i = 0; i < indent; ++i)
        os << "  ";
    bool is_dir = f.is_directory();
    os << "- " << name;
    if(is_dir && name != "/")
        os << '/';
    os << '\n';

    if(is_dir)
    {
        auto* dir = f.get_if<file_node::directory>();
        assert(dir != nullptr);
        for(const auto& [sub_name, sub_f] : dir->children())
        {
            list_files_impl(os, sub_name, sub_f, indent + 1);
        }
    }
}

const file_node* virtual_file_system::find_impl(path_view p) const
{
    if(p.empty() || !p.is_absolute()) [[unlikely]]
        return nullptr;
    if(p == "/"_pv)
        return m_root.get();

    const file_node* current = m_root.get();
    for(path_view subview : p.split_view())
    {
        auto* dir = current->get_if<file_node::directory>();
        if(!dir)
            return nullptr;

        auto it = dir->children().find(std::string_view(subview));
        if(it == dir->children().end())
            return nullptr;

        current = &it->second;
    }

    return current;
}

const file_node* virtual_file_system::mkdir_impl(path_view p)
{
    const file_node* current = m_root.get();
    for(path_view subview : p.split_view())
    {
        assert(current->is_directory());
        auto* dir = current->get_if<file_node::directory>();
        assert(dir);

        auto it = dir->children().find(std::string_view(subview));
        if(it != dir->children().end())
        {
            if(!it->second.is_directory())
            {
                throw error(vfs_err_msg(subview, " already exists"));
            }
        }
        else
        {
            it = dir->children().emplace_hint(
                it,
                subview.string(),
                file_node(current, std::in_place_type<file_node::directory>)
            );
        }

        current = &it->second;
    }

    return current;
}

template <typename T, typename... Args>
auto virtual_file_system::mount_impl(
    path_view p, bool overwrite, std::in_place_type_t<T>, Args&&... args
) -> std::pair<const file_node*, bool>
{
    static_assert(!std::same_as<T, file_node::directory>, "Cannot mount a directory");
    assert(p.is_absolute());
    const file_node* current = mkdir_impl(p.parent_path());
    path_view filename = p.filename();

    auto* dir = current->get_if<file_node::directory>();
    assert(dir);
    auto it = dir->children().find(filename);
    if(it != dir->children().end())
    {
        if(overwrite)
        {
            it->second = file_node(
                current, std::in_place_type<T>, std::forward<Args>(args)...
            );

            return std::make_pair(&it->second, true);
        }

        return std::make_pair(&it->second, false);
    }
    else
    {
        auto result = dir->children().emplace(
            filename.string(),
            file_node(current, std::in_place_type<T>, std::forward<Args>(args)...)
        );
        assert(result.second); // Emplacement should be successful here

        return std::make_pair(
            &result.first->second,
            true
        );
    }
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
