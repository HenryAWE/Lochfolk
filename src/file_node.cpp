#include "file_node.hpp"
#include <memory>
#include <utility>
#include <lochfolk/vfs.hpp>
#include "errmsg.hpp"
#include "fh/bytes.hpp"
#include "fh/file.hpp"

namespace lochfolk
{
namespace file_data
{
    std::unique_ptr<file_handle> string_constant::get_fh(file_flag) const
    {
        // Copy the contents so that the handle does not alias the node.
        return make_fh_bytes(std::string(view()));
    }

    std::uint64_t string_constant::file_size() const
    {
        return std::visit(
            [](const auto& v) -> std::uint64_t
            { return static_cast<std::uint64_t>(v.size()); },
            m_str_data
        );
    }

    std::string_view string_constant::view() const noexcept
    {
        return std::visit(
            [](const auto& v) -> std::string_view
            { return v; },
            m_str_data
        );
    }

    std::unique_ptr<file_handle> sys_file::get_fh(file_flag flags) const
    {
        // Throws std::system_error if the file cannot be opened.
        return std::make_unique<fh_file>(m_sys_path, flags);
    }

    std::uint64_t sys_file::file_size() const
    {
        return static_cast<std::uint64_t>(
            std::filesystem::file_size(m_sys_path)
        );
    }

    archive_entry::archive_entry(archive& ar, std::int64_t off)
        : m_archive_ref(ar.shared_from_this()), m_offset(off)
    {}

    archive_entry& archive_entry::operator=(
        archive_entry&& rhs
    ) noexcept
    {
        if(this == &rhs) [[unlikely]]
            return *this;

        m_archive_ref = std::move(rhs.m_archive_ref);
        m_offset = std::exchange(rhs.m_offset, 0);

        return *this;
    }

    std::unique_ptr<file_handle> archive_entry::get_fh(file_flag) const
    {
        return m_archive_ref->get_fh_of(m_offset);
    }

    std::uint64_t archive_entry::file_size() const
    {
        return m_archive_ref->get_file_size(m_offset);
    }
} // namespace file_data

namespace detail
{
    bool file_node::is_directory() const noexcept
    {
        return std::holds_alternative<file_data::directory>(m_data);
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

    std::unique_ptr<file_handle> file_node::get_fh(file_flag flags) const
    {
        return visit(
            [flags](const auto& v) -> std::unique_ptr<file_handle>
            {
                return v.get_fh(flags);
            }
        );
    }
} // namespace detail

detail::file_node* find_impl(detail::file_node& root, path_view p)
{
    return const_cast<detail::file_node*>(
        find_impl(std::as_const(root), p)
    );
}

const detail::file_node* find_impl(const detail::file_node& root, path_view p)
{
    if(p.empty() || !p.is_absolute()) [[unlikely]]
        return nullptr;
    if(p == "/"_pv)
        return &root;

    const auto* current = &root;
    for(path_view subview : p)
    {
        if(std::string_view(subview) == "/")
        {
            current = &root;
            continue;
        }

        auto* dir = current->get_if<file_data::directory>();
        if(!dir)
            return nullptr;

        auto it = dir->children().find(std::string_view(subview));
        if(it == dir->children().end())
            return nullptr;

        current = &it->second;
    }

    return current;
}

detail::file_node* mkdir_impl(detail::file_node& root, path_view p)
{
    assert(root.is_directory());

    auto* current = &root;
    for(path_view subview : p)
    {
        if(std::string_view(subview) == "/")
            continue;

        assert(current->is_directory());
        auto* dir = current->get_if<file_data::directory>();
        assert(dir);

        auto it = dir->children().find(std::string_view(subview));
        if(it != dir->children().end())
        {
            if(!it->second.is_directory())
            {
                throw virtual_file_system::error(vfs_err_msg(subview, " already exists"));
            }
        }
        else
        {
            it = dir->children().emplace_hint(
                it,
                subview.string(),
                detail::file_node(current, std::in_place_type<file_data::directory>)
            );
        }

        current = &it->second;
    }

    return current;
}

void list_files_impl(
    std::ostream& os,
    std::string_view name,
    const detail::file_node& f,
    unsigned int indent
)
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
        auto* dir = f.get_if<file_data::directory>();
        assert(dir != nullptr);
        for(const auto& [sub_name, sub_f] : dir->children())
        {
            list_files_impl(os, sub_name, sub_f, indent + 1);
        }
    }
}
} // namespace lochfolk
