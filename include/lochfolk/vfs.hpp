#ifndef LOCHFOLK_VFS_HPP
#define LOCHFOLK_VFS_HPP

#pragma once

#include <cstdint>
#include <ios>
#include <iosfwd>
#include <memory>
#include <stdexcept>
#include <string>
#include <filesystem>
#include "detail/config.hpp"
#include "path.hpp"
#include "stream.hpp"
#include "io.hpp"

namespace lochfolk
{
namespace detail
{
    class file_node;
} // namespace detail

class virtual_file_system
{
    struct vfs_data;

public:
    class error : public std::runtime_error
    {
    public:
        using runtime_error::runtime_error;
    };

    LOCHFOLK_API virtual_file_system();
    LOCHFOLK_API virtual_file_system(const virtual_file_system&) = delete;

    LOCHFOLK_API ~virtual_file_system();

    LOCHFOLK_API void mount_string(
        path_view p, std::string_view str, bool overwrite = true
    );
    LOCHFOLK_API void mount_string(
        path_view p, const char* str, bool overwrite = true
    );

    LOCHFOLK_API void mount_string(
        path_view p, std::string str, bool overwrite = true
    );

    LOCHFOLK_API void mount_file(
        path_view p, const std::filesystem::path& sys_path, bool overwrite = true
    );

    /**
     * @brief Recursively mount a directory
     *
     * @param dir Must be a directory
     */
    LOCHFOLK_API void mount_dir(
        path_view p, const std::filesystem::path& dir, bool overwrite = true
    );

    LOCHFOLK_API void mount_archive(
        path_view p, const std::filesystem::path& sys_path, bool overwrite = true
    );

    [[nodiscard]]
    LOCHFOLK_API bool exists(path_view p) const;

    [[nodiscard]]
    LOCHFOLK_API bool is_directory(path_view p) const;

    [[nodiscard]]
    LOCHFOLK_API std::uint64_t file_size(path_view p) const;

    LOCHFOLK_API bool remove(path_view p);

    /**
     * @brief Open a virtual file
     *
     * The returned handle is owned by the caller and stays usable after the
     * virtual file system is modified or destroyed.
     *
     * `flags` selects the access modes. Only files mounted from the host
     * file system can be opened for writing: string constants and archive
     * entries are always read-only, so a request for `file_flag::writable`
     * is silently reduced to `file_flag::readable` for them (check
     * `file_handle::writable()` or the result of `file_handle::write()`).
     *
     * If @p convert_crlf is `true`, CRLF sequences are translated to LF
     * while reading. The conversion is applied to every kind of entry and
     * is independent of the host platform.
     *
     * @throw virtual_file_system::error if the path does not exist or is a
     * directory
     * @throw std::invalid_argument if no access mode is requested
     * @throw std::system_error if a host file cannot be opened
     */
    LOCHFOLK_API std::unique_ptr<file_handle> fopen(
        path_view p,
        file_flag flags = file_flag::readable,
        bool convert_crlf = false
    );

    /**
     * @brief Open a virtual file as an input stream
     *
     * The stream reads through a `file_handle`; no `std::filebuf` or
     * `std::stringbuf` is involved.
     *
     * Text mode is the default (as for `std::ifstream`): CRLF is translated
     * to LF, identically on every platform. Pass `std::ios_base::binary` to
     * read the raw bytes. `tellg()` and `seekg()` always use the raw byte
     * offsets of the underlying file, so a CRLF pair advances the position by
     * two.
     *
     * @param p Path
     * @param mode `in` (text, the default) or `in|binary`; requesting
     * `out`/`app`/`trunc` throws `std::invalid_argument`
     *
     * @throw virtual_file_system::error if the path does not exist or is a
     * directory
     * @throw std::invalid_argument if a write mode is requested
     * @throw std::system_error if a host file cannot be opened
     */
    LOCHFOLK_API ivfstream open(
        path_view p, std::ios_base::openmode mode = std::ios_base::in
    );

    /**
     * @brief Read a whole virtual file into a string
     *
     * @param p Path
     * @param convert_crlf Convert CRLF to LF while reading
     */
    [[nodiscard]]
    LOCHFOLK_API std::string read_string(path_view p, bool convert_crlf = true);

    /**
     * @brief List all files for debugging
     */
    LOCHFOLK_API void list_files(std::ostream& os);

private:
    vfs_data* m_vfs_data;
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
    std::unique_ptr<file_handle> fopen(
        path_view p,
        file_flag flags = file_flag::readable,
        bool convert_crlf = false
    ) const
    {
        return m_vfs->fopen(to_fullpath(p), flags, convert_crlf);
    }

    [[nodiscard]]
    ivfstream open(
        path_view p, std::ios_base::openmode mode = std::ios_base::in
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
