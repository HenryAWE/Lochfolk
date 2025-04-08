#ifndef LOCHFOLK_VFS_HPP
#define LOCHFOLK_VFS_HPP

#pragma once

#include <ios>
#include <map>
#include <stdexcept>
#include <memory>
#include <filesystem>
#include <variant>
#include <utility>
#include "path.hpp"
#include "archive.hpp"
#include "stream.hpp"

namespace lochfolk
{
class virtual_file_system
{
public:
    class file_node;

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

        file_node& operator=(file_node&& rhs) noexcept
        {
            if(this == &rhs) [[unlikely]]
                return *this;

            m_parent = rhs.m_parent;
            m_data = std::move(rhs.m_data);
            return *this;
        }

        struct directory
        {
            directory() = default;

            directory(directory&&) noexcept = default;

            directory(container_type c) noexcept
                : children(std::move(c)) {}

            directory& operator=(directory&& rhs) noexcept = default;

            container_type children;
        };

        struct string_constant
        {
            using string_data = std::variant<
                std::string,
                std::string_view>;

            string_data str;

            string_constant(string_constant&&) noexcept = default;

            string_constant(std::string str_) noexcept
                : str(std::in_place_index<0>, std::move(str_)) {}

            string_constant(std::string_view str_) noexcept
                : str(std::in_place_index<1>, str_) {}

            string_constant& operator=(string_constant&& rhs) noexcept = default;

            std::unique_ptr<std::streambuf> open(std::ios_base::openmode) const;
        };

        struct sys_file
        {
            std::filesystem::path sys_path;

            sys_file(sys_file&&) noexcept = default;

            sys_file(std::filesystem::path sys_path)
                : sys_path(std::move(sys_path))
            {}

            sys_file& operator=(sys_file&& rhs) noexcept = default;

            std::unique_ptr<std::filebuf> open(std::ios_base::openmode mode) const;
        };

        struct archive_entry
        {
            std::shared_ptr<archive> archive_ref;
            std::int64_t offset;

            archive_entry(archive_entry&&) noexcept = default;

            archive_entry(std::shared_ptr<archive> ar, std::int64_t off)
                : archive_ref(std::move(ar)), offset(off) {}

            archive_entry& operator=(archive_entry&& rhs) noexcept
            {
                if(this == &rhs) [[unlikely]]
                    return *this;

                archive_ref = std::move(rhs.archive_ref);
                offset = std::exchange(rhs.offset, 0);

                return *this;
            }

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

    ~virtual_file_system();

    void mount_string_constant(
        path_view p, std::string_view str, bool overwrite = true
    );
    void mount_string_constant(
        path_view p, const char* str, bool overwrite = true
    );

    void mount_string_constant(
        path_view p, std::string str, bool overwrite = true
    );

    void mount_sys_file(
        path_view p, const std::filesystem::path& sys_path, bool overwrite = true
    );

    /**
     * @brief Recursively mount a directory
     *
     * @param dir Must be a directory
     */
    void mount_sys_dir(
        path_view p, const std::filesystem::path& dir, bool overwrite = true
    );

    void mount_zip_archive(
        path_view p, const std::filesystem::path& sys_path, bool overwrite = true
    );

    bool exists(path_view p) const;

    bool is_directory(path_view p) const;

    ivfstream open(
        path_view p, std::ios_base::openmode mode = std::ios_base::binary
    );

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
    auto mount_impl(path_view p, bool overwrite, std::in_place_type_t<T>, Args&&... args)
        -> std::pair<const file_node*, bool>;
};
} // namespace lochfolk

#endif
