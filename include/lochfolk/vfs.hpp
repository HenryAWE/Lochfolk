#ifndef LOCHFOLK_VFS_HPP
#define LOCHFOLK_VFS_HPP

#pragma once

#include <ios>
#include <stdexcept>
#include <memory>
#include <filesystem>
#include <utility>
#include "detail/config.hpp"
#include "path.hpp"
#include "stream.hpp"

namespace lochfolk
{
class file_node;

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
    std::unique_ptr<file_node> m_root;

    void list_files_impl(std::ostream& os, std::string_view name, const file_node& f, unsigned int indent);

    const file_node* find_impl(path_view p) const;

    const file_node* mkdir_impl(path_view p);

    template <typename T, typename... Args>
    auto mount_impl(path_view p, bool overwrite, std::in_place_type_t<T>, Args&&... args)
        -> std::pair<const file_node*, bool>;
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
