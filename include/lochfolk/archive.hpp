#ifndef LOCHFOLK_ARCHIVE_HPP
#define LOCHFOLK_ARCHIVE_HPP

#pragma once

#include <cstdint>
#include <cstddef>
#include <span>
#include <iostream>
#include <memory>
#include <filesystem>

namespace lochfolk
{
class archive : public std::enable_shared_from_this<archive>
{
protected:
    archive();

public:
    virtual ~archive();

    virtual std::unique_ptr<std::streambuf> getbuf(
        std::int64_t offset, std::ios_base::openmode mode
    ) const = 0;

    virtual std::uint64_t get_file_size(
        std::int64_t offset
    ) const = 0;
};

class zip_archive : public archive
{
public:
    class minizip_error : public std::runtime_error
    {
    public:
        minizip_error(std::int32_t err);

        static std::string translate_error(std::int32_t err);

        [[nodiscard]]
        std::int32_t error() const noexcept
        {
            return m_err;
        }

    private:
        std::int32_t m_err;
    };

    zip_archive();

    zip_archive(const zip_archive&) = delete;

    ~zip_archive();

    std::unique_ptr<std::streambuf> getbuf(
        std::int64_t offset, std::ios_base::openmode mode
    ) const override;

    std::uint64_t get_file_size(std::int64_t offset) const override;

    void open(const std::filesystem::path& sys_path);

    void close();

    bool goto_first() const;

    /**
     * @brief Goto next entry
     *
     * @return true No error
     * @return false End of file list
     */
    bool goto_next() const;

    void goto_entry(std::int64_t offset) const;

    bool entry_is_dir() const;

    std::int64_t get_entry_offset() const;

    std::unique_ptr<std::streambuf> get_entry_buf(std::ios_base::openmode mode) const;

    void open_entry() const;

    std::size_t read_entry(std::span<std::byte> buf) const;

    /**
     * @brief Uncompressed file size
     */
    std::uint64_t entry_file_size() const;

    std::string_view entry_filename() const;

    void close_entry() const noexcept;

private:
    struct stream_deleter
    {
        void operator()(void* stream) const noexcept;
    };

    void* m_handle;
    std::unique_ptr<void, stream_deleter> m_stream;
};
} // namespace lochfolk

#endif
