#pragma once

#include <cassert>
#include <cstdint>
#include <iosfwd>
#include <map>
#include <string>
#include <variant>
#include <memory>
#include <filesystem>
#include <lochfolk/path.hpp>
#include "archive.hpp"

namespace lochfolk
{
namespace detail
{
    class file_node;
}

struct string_compare
{
    using is_transparent = void;

    bool operator()(std::string_view lhs, std::string_view rhs) const
    {
        return lhs < rhs;
    }

    bool operator()(path_view lhs, std::string_view rhs) const
    {
        return std::string_view(lhs) < rhs;
    }

    bool operator()(std::string_view lhs, path_view rhs) const
    {
        return lhs < std::string_view(rhs);
    }
};

namespace file_data
{
    using file_container_type = std::map<std::string, detail::file_node, string_compare>;

    class directory
    {
    public:
        directory() = default;

        directory(directory&&) noexcept = default;

        directory(file_container_type c) noexcept
            : m_children(std::move(c)) {}

        directory& operator=(directory&& rhs) noexcept = default;

        /**
         * @brief Always returns 0 as a placeholder
         */
        std::uint64_t file_size() const noexcept
        {
            return 0;
        }

        file_container_type& children() noexcept
        {
            return m_children;
        }

        const file_container_type& children() const noexcept
        {
            return m_children;
        }

    private:
        file_container_type m_children;
    };

    class string_constant
    {
    public:
        using string_data = std::variant<
            std::string,
            std::string_view>;

        string_constant(string_constant&&) noexcept = default;

        string_constant(std::string str) noexcept
            : m_str_data(std::in_place_index<0>, std::move(str)) {}

        string_constant(std::string_view str) noexcept
            : m_str_data(std::in_place_index<1>, str) {}

        string_constant& operator=(string_constant&& rhs) noexcept = default;

        std::unique_ptr<std::streambuf> open(std::ios_base::openmode mode) const;

        std::string read_string(bool convert_crlf) const;

        std::uint64_t file_size() const;

        [[nodiscard]]
        std::string_view view() const noexcept;

    private:
        string_data m_str_data;
    };

    class sys_file
    {
    public:
        sys_file(sys_file&&) noexcept = default;

        sys_file(std::filesystem::path sys_path)
            : m_sys_path(std::move(sys_path))
        {}

        sys_file& operator=(sys_file&& rhs) noexcept = default;

        std::unique_ptr<std::filebuf> open(std::ios_base::openmode mode) const;

        std::string read_string(bool convert_crlf) const;

        std::uint64_t file_size() const;

        const std::filesystem::path& system_path() const noexcept
        {
            return m_sys_path;
        }

    private:
        std::filesystem::path m_sys_path;
    };

    struct archive_entry
    {
    public:
        archive_entry(archive_entry&&) noexcept = default;

        archive_entry(archive& ar, std::int64_t off);

        archive_entry& operator=(archive_entry&& rhs) noexcept;

        std::unique_ptr<std::streambuf> open(std::ios_base::openmode mode) const;

        std::string read_string(bool convert_crlf) const;

        std::uint64_t file_size() const;

    private:
        std::shared_ptr<archive> m_archive_ref;
        std::int64_t m_offset;
    };
} // namespace file_data

namespace detail
{
    class file_node
    {
        friend class virtual_file_system;

    public:
        file_node() = delete;

        template <typename T, typename... Args>
        file_node(const file_node* parent, std::in_place_type_t<T>, Args&&... args)
            : m_parent(parent),
              m_data(std::in_place_type<T>, std::forward<Args>(args)...)
        {}

        file_node(file_node&&) noexcept = default;

        file_node& operator=(file_node&& rhs) noexcept
        {
            if(this == &rhs) [[unlikely]]
                return *this;

            m_parent = rhs.m_parent;
            m_data = std::move(rhs.m_data);
            return *this;
        }

        using data_type = std::variant<
            file_data::directory,
            file_data::string_constant,
            file_data::sys_file,
            file_data::archive_entry>;

        template <typename Visitor>
        decltype(auto) visit(Visitor&& vis) const
        {
            return std::visit(std::forward<Visitor>(vis), m_data);
        }

        template <typename T>
        T* get_if() const noexcept
        {
            return std::get_if<T>(&m_data);
        }

        [[nodiscard]]
        bool is_directory() const noexcept;

        std::uint64_t file_size() const;

        std::unique_ptr<std::streambuf> getbuf(std::ios_base::openmode mode) const;

        std::string read_string(bool convert_crlf = true) const;

    private:
        const file_node* m_parent;
        mutable data_type m_data;
    };
} // namespace detail

const detail::file_node* find_impl(const detail::file_node& root, path_view p);

const detail::file_node* mkdir_impl(detail::file_node& root, path_view p);

template <typename T, typename... Args>
std::pair<const detail::file_node*, bool> mount_impl(
    detail::file_node& root,
    path_view p,
    bool overwrite,
    std::in_place_type_t<T>,
    Args&&... args
)
{
    static_assert(!std::same_as<T, file_data::directory>, "Cannot mount a directory");
    assert(p.is_absolute());
    const auto* current = mkdir_impl(root, p.parent_path());
    path_view filename = p.filename();

    auto* dir = current->get_if<file_data::directory>();
    assert(dir);
    auto it = dir->children().find(filename);
    if(it != dir->children().end())
    {
        if(overwrite)
        {
            it->second = detail::file_node(
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
            detail::file_node(current, std::in_place_type<T>, std::forward<Args>(args)...)
        );
        assert(result.second); // Emplacement should be successful here

        return std::make_pair(
            &result.first->second,
            true
        );
    }
}

void list_files_impl(
    std::ostream& os,
    std::string_view name,
    const detail::file_node& f,
    unsigned int indent
);
} // namespace lochfolk
