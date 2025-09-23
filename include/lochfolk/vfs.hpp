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
#include "detail/config.hpp"
#include "path.hpp"
#include "archive.hpp"
#include "stream.hpp"

namespace lochfolk
{
class virtual_file_system;

namespace detail
{
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

    class file_node;

    using file_container_type = std::map<std::string, file_node, string_compare>;

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

        class directory
        {
        public:
            directory() = default;

            directory(directory&&) noexcept = default;

            directory(file_container_type c) noexcept
                : m_children(std::move(c)) {}

            directory& operator=(directory&& rhs) noexcept = default;

            /**
             * @brief Always returns 0
             */
            std::uint64_t file_size() const noexcept;

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

        [[nodiscard]]
        LOCHFOLK_API bool is_directory() const noexcept;

        LOCHFOLK_API std::uint64_t file_size() const;

        LOCHFOLK_API std::unique_ptr<std::streambuf> getbuf(std::ios_base::openmode mode) const;

        LOCHFOLK_API std::string read_string(bool convert_crlf = true) const;

    private:
        const file_node* m_parent;
        mutable data_type m_data;
    };
} // namespace detail

class virtual_file_system
{
public:
    class error : public std::runtime_error
    {
    public:
        using runtime_error::runtime_error;
    };

    LOCHFOLK_API virtual_file_system();
    LOCHFOLK_API virtual_file_system(const virtual_file_system&) = delete;

    LOCHFOLK_API ~virtual_file_system();

    LOCHFOLK_API void mount_string_constant(
        path_view p, std::string_view str, bool overwrite = true
    );
    LOCHFOLK_API void mount_string_constant(
        path_view p, const char* str, bool overwrite = true
    );

    LOCHFOLK_API void mount_string_constant(
        path_view p, std::string str, bool overwrite = true
    );

    LOCHFOLK_API void mount_sys_file(
        path_view p, const std::filesystem::path& sys_path, bool overwrite = true
    );

    /**
     * @brief Recursively mount a directory
     *
     * @param dir Must be a directory
     */
    LOCHFOLK_API void mount_sys_dir(
        path_view p, const std::filesystem::path& dir, bool overwrite = true
    );

    LOCHFOLK_API void mount_zip_archive(
        path_view p, const std::filesystem::path& sys_path, bool overwrite = true
    );

    [[nodiscard]]
    LOCHFOLK_API bool exists(path_view p) const;

    [[nodiscard]]
    LOCHFOLK_API bool is_directory(path_view p) const;

    [[nodiscard]]
    LOCHFOLK_API std::uint64_t file_size(path_view p) const;

    LOCHFOLK_API bool remove(path_view p);

    LOCHFOLK_API ivfstream open(
        path_view p, std::ios_base::openmode mode = std::ios_base::binary
    );

    /**
     * @brief Read string from virtual file
     *
     * @param p Path
     * @param convert_crlf Convert CRLF to LF for system files on Windows
     */
    [[nodiscard]]
    LOCHFOLK_API std::string read_string(path_view p, bool convert_crlf = true);

    /**
     * @brief List all files for debugging
     */
    LOCHFOLK_API void list_files(std::ostream& os);

private:
    detail::file_node m_root;

    void list_files_impl(std::ostream& os, std::string_view name, const detail::file_node& f, unsigned int indent);

    const detail::file_node* find_impl(path_view p) const;

    const detail::file_node* mkdir_impl(path_view p);

    template <typename T, typename... Args>
    auto mount_impl(path_view p, bool overwrite, std::in_place_type_t<T>, Args&&... args)
        -> std::pair<const detail::file_node*, bool>;
};

class access_context
{
public:
    access_context() = delete;

    LOCHFOLK_API access_context(access_context&& other) noexcept;

    LOCHFOLK_API access_context(const access_context&);

    LOCHFOLK_API access_context(virtual_file_system& vfs);

    [[nodiscard]]
    const path& current_path() const noexcept
    {
        return m_current;
    }

    LOCHFOLK_API void current_path(path_view pv);

    LOCHFOLK_API path to_fullpath(path_view pv) const;

    [[nodiscard]]
    virtual_file_system& get_vfs() const noexcept
    {
        return *m_vfs;
    }

    [[nodiscard]]
    bool exists(path_view p) const
    {
        return m_vfs->exists(to_fullpath(p));
    }

    [[nodiscard]]
    bool is_directory(path_view p) const
    {
        return m_vfs->is_directory(to_fullpath(p));
    }

    [[nodiscard]]
    std::uint64_t file_size(path_view p) const
    {
        return m_vfs->file_size(to_fullpath(p));
    }

    bool remove(path_view p) const
    {
        return m_vfs->remove(to_fullpath(p));
    }

    [[nodiscard]]
    ivfstream open(
        path_view p, std::ios_base::openmode mode = std::ios_base::binary
    ) const
    {
        return m_vfs->open(to_fullpath(p), mode);
    }

    [[nodiscard]]
    std::string read_string(
        path_view p, bool convert_crlf = true
    )
    {
        return m_vfs->read_string(to_fullpath(p), convert_crlf);
    }

private:
    virtual_file_system* m_vfs;
    path m_current;
};
} // namespace lochfolk

#endif
