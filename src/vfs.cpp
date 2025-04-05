#include <lochfolk/vfs.hpp>
#include <memory>
#include <cassert>
#include <fstream>
#include <lochfolk/utility.hpp>
#include "zip_archive.hpp"

namespace lochfolk
{
std::unique_ptr<std::streambuf> virtual_file_system::file_node::string_constant::open(
    std::ios_base::openmode mode
) const
{
    mode &= ~std::ios_base::out;

    auto sp = std::visit(
        []<typename T>(T&& v) -> std::span<char>
        {
            if constexpr(std::same_as<std::remove_cvref_t<T>, std::string_view>)
            {
                return std::span<char>(const_cast<char*>(v.data()), v.size());
            }
            else // std::shared_ptr<std::string>
            {
                if(!v) [[unlikely]]
                    return std::span<char>();

                // TODO: guard ownership
                return *v;
            }
        },
        str
    );

    return std::make_unique<span_buf>(sp, mode);
}

std::unique_ptr<std::filebuf> virtual_file_system::file_node::sys_file::open(
    std::ios_base::openmode mode
) const
{
    std::unique_ptr fb = std::make_unique<std::filebuf>();
    fb->open(sys_path, mode);
    if(!fb->is_open())
        throw error("failed to open ");

    return fb;
}

std::unique_ptr<std::streambuf> virtual_file_system::file_node::archive_entry::open(
    std::ios_base::openmode mode
) const
{
    return archive_ref->getbuf(offset, mode);
}

bool virtual_file_system::file_node::is_directory() const noexcept
{
    return std::holds_alternative<directory>(m_data);
}

std::unique_ptr<std::streambuf> virtual_file_system::file_node::getbuf(std::ios_base::openmode mode) const
{
    return visit(
        [mode]<typename T>(const T& data) -> std::unique_ptr<std::streambuf>
        {
            constexpr bool has_buf = requires() { data.open(mode); };
            if constexpr(has_buf)
                return data.open(mode);
            throw error("bad file");
        }
    );
}

virtual_file_system::virtual_file_system()
    : m_root(nullptr, std::in_place_type<file_node::directory>)
{
    assert(m_root.is_directory());
}

virtual_file_system::~virtual_file_system() = default;

void virtual_file_system::mount_string_constant(
    path_view p, std::string_view str, bool overwrite
)
{
    mount_impl(
        p,
        false,
        std::in_place_type<file_node::string_constant>,
        str
    );
}

void virtual_file_system::mount_string_constant(
    path_view p, std::shared_ptr<std::string> str, bool overwrite
)
{
    mount_impl(
        p,
        false,
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
        throw error("not exist");
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
        throw error("not a regular file");
    }
}

void virtual_file_system::mount_sys_dir(
    path_view p, const std::filesystem::path& dir, bool overwrite
)
{
    namespace stdfs = std::filesystem;

    if(!stdfs::is_directory(dir))
        throw error("is not a directory");

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
        if(ar->entry_is_dir())
            continue;
        ar->open_entry();
        try
        {
            const mz_zip_file& info = ar->get_entry_info();
            std::string_view filename(info.filename, info.filename_size);

            mount_impl(
                base / filename,
                overwrite,
                std::in_place_type<file_node::archive_entry>,
                ar,
                ar->get_entry_offset()
            );
        }
        catch(...)
        {
            ar->close_entry();
            throw;
        }
        ar->close_entry();
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

ivfstream virtual_file_system::read(path_view p, std::ios_base::openmode mode)
{
    const file_node* f = find_impl(p);
    if(!f)
        throw error("not found");

    mode |= std::ios_base::in;
    return ivfstream(f->getbuf(mode));
}

void virtual_file_system::list_files(std::ostream& os)
{
    list_files_impl(os, "/", m_root, 0);
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
        for(const auto& [sub_name, sub_f] : dir->children)
        {
            list_files_impl(os, sub_name, sub_f, indent + 1);
        }
    }
}

const virtual_file_system::file_node* virtual_file_system::find_impl(path_view p) const
{
    if(p.empty() || !p.is_absolute()) [[unlikely]]
        return nullptr;
    if(p == "/"_pv)
        return &m_root;

    const file_node* current = &m_root;
    for(path_view subview : p.split_view())
    {
        auto* dir = current->get_if<file_node::directory>();
        if(!dir)
            return nullptr;

        auto it = dir->children.find(std::string_view(subview));
        if(it == dir->children.end())
            return nullptr;

        current = &it->second;
    }

    return current;
}

const virtual_file_system::file_node* virtual_file_system::mkdir_impl(path_view p)
{
    const file_node* current = &m_root;
    for(path_view subview : p.split_view())
    {
        assert(current->is_directory());
        auto* dir = current->get_if<file_node::directory>();
        assert(dir);

        auto it = dir->children.find(std::string_view(subview));
        if(it != dir->children.end())
        {
            if(!it->second.is_directory())
            {
                throw error("already exists");
            }
        }
        else
        {
            it = dir->children.emplace_hint(
                it,
                std::string(subview),
                file_node(current, std::in_place_type<file_node::directory>)
            );
        }

        current = &it->second;
    }

    return current;
}

template <typename T, typename... Args>
auto virtual_file_system::mount_impl(path_view p, bool overwrite, std::in_place_type_t<T>, Args&&... args)
    -> std::pair<const file_node*, bool>
{
    static_assert(!std::same_as<T, file_node::directory>, "Cannot mount a directory");
    assert(p.is_absolute());
    const file_node* current = mkdir_impl(p.parent_path());
    path_view filename = p.filename();

    auto* dir = current->get_if<file_node::directory>();
    assert(dir);
    auto it = dir->children.find(filename);
    if(it != dir->children.end())
    {
        if(overwrite)
        {
            it->second = file_node(
                current, std::in_place_type<T>, std::forward<Args>(args)...
            );
        }

        return std::make_pair(&it->second, false);
    }
    else
    {
        auto result = dir->children.emplace(
            filename,
            file_node(current, std::in_place_type<T>, std::forward<Args>(args)...)
        );
        assert(result.second); // Emplacement should be successful here

        return std::make_pair(
            &result.first->second,
            true
        );
    }
}
} // namespace lochfolk
