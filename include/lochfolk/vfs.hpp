#ifndef LOCHFOLK_VFS_HPP
#define LOCHFOLK_VFS_HPP

#pragma once

#include <cassert>
#include <ios>
#include <map>
#include <stdexcept>
#include <memory>
#include <filesystem>
#include <variant>
#include "path.hpp"
#include "archive.hpp"

namespace lochfolk
{
class virtual_file_system
{
public:
    struct file_node;

    struct string_compare
    {
        using is_transparent = void;

        constexpr bool operator()(std::string_view lhs, std::string_view rhs) const
        {
            return lhs < rhs;
        }
    };

    using container_type = std::map<std::string, file_node, string_compare>;

    class error : public std::runtime_error
    {
    public:
        using runtime_error::runtime_error;
    };

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

        struct directory
        {
            container_type children;
        };

        struct string_constant
        {
            std::string str;

            string_constant(std::string str)
                : str(std::move(str)) {}

            std::unique_ptr<std::stringbuf> open(std::ios_base::openmode) const;
        };

        struct sys_file
        {
            std::filesystem::path sys_path;

            sys_file(std::filesystem::path sys_path)
                : sys_path(std::move(sys_path)) {}

            std::unique_ptr<std::filebuf> open(std::ios_base::openmode mode) const;
        };

        struct archive_entry
        {
            std::shared_ptr<archive> archive_ref;
            std::int64_t offset;

            std::unique_ptr<std::streambuf> open(std::ios_base::openmode mode) const;
        };

        using data_type = std::variant<
            directory,
            string_constant,
            sys_file,
            archive_entry>;

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

        bool is_directory() const noexcept;

        std::unique_ptr<std::streambuf> getbuf(std::ios_base::openmode mode) const;

    private:
        const file_node* m_parent;
        mutable data_type m_data;
    };

    virtual_file_system();
    virtual_file_system(const virtual_file_system&) = delete;

    void mount_string_constant(path_view p, std::string str);

    void mount_sys_file(path_view p, const std::filesystem::path& sys_path);

    /**
     * @brief Recursively mount a directory
     *
     * @param dir Must be a directory
     */
    void mount_sys_dir(path_view p, const std::filesystem::path& dir);

    void mount_zip_archive(path_view p, const std::filesystem::path& sys_path);

    bool exists(path_view p) const;

    bool is_directory(path_view p) const;

    ivfstream read(path_view p, std::ios_base::openmode mode = std::ios_base::binary);

    /**
     * @brief List all files for debugging
     */
    void list_files(std::ostream& os);

private:
    file_node m_root;

    void list_files_impl(std::ostream& os, std::string_view name, const file_node& f, unsigned int indent);

    const file_node* find_impl(path_view p) const;

    const file_node* mkdir_impl(path_view p);

    template <typename T, typename... Args>
    std::pair<const file_node*, bool> mount_impl(path_view p, std::in_place_type_t<T>, Args&&... args)
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
            // TODO: Allowing for overwriting an existed file based on user decision   return std::make_pair(&it->second, false);

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
};
} // namespace lochfolk

#endif
