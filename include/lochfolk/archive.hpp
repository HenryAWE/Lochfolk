#ifndef LOCHFOLK_ARCHIVE_HPP
#define LOCHFOLK_ARCHIVE_HPP

#pragma once

#include <cstdint>
#include <cstddef>
#include <span>
#include <vector>
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

    std::unique_ptr<std::streambuf> getbuf(
        std::int64_t offset, std::ios_base::openmode mode
    ) const;

    virtual std::string read_string(std::int64_t offset) const = 0;
    virtual std::vector<std::byte> read_bytes(std::int64_t offset) const = 0;

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

    std::string read_string(std::int64_t offset) const override;
    std::vector<std::byte> read_bytes(std::int64_t offset) const override;

    std::uint64_t get_file_size(std::int64_t offset) const override;

    void open(const std::filesystem::path& sys_path);

    void close() noexcept;

    bool goto_first() const;

    /**
     * @brief Goto next entry
     *
     * @return true No error
     * @return false End of file list
     */
    bool goto_next() const;

    bool current_is_dir() const;

    std::int64_t current_offset() const;

    /**
     * @brief RAII helper for opening an entry
     */
    class current_entry
    {
        friend class zip_archive;

        current_entry(const zip_archive& ar)
            : _this(&ar)
        {
            _this->open_entry();
        }

    public:
        current_entry() = delete;
        current_entry(const current_entry&) = delete;

        ~current_entry()
        {
            _this->close_entry();
        }

        [[nodiscard]]
        std::uint64_t file_size() const
        {
            return _this->entry_file_size();
        }

        [[nodiscard]]
        std::string_view filename() const
        {
            return _this->entry_filename();
        }

        [[nodiscard]]
        std::int64_t offset() const
        {
            return _this->current_offset();
        }

        std::size_t read(std::span<std::byte> buf) const
        {
            return _this->read_entry(buf);
        }

    private:
        const zip_archive* _this;
    };

    [[nodiscard]]
    current_entry open_current() const
    {
        return current_entry(*this);
    }

private:
    void open_entry() const;
    void close_entry() const noexcept;

    std::size_t read_entry(std::span<std::byte> buf) const;

    /**
     * @brief Uncompressed file size
     */
    std::uint64_t entry_file_size() const;

    std::string_view entry_filename() const;

    void goto_entry(std::int64_t offset) const;

    struct handle_deleter
    {
        void operator()(void* stream) const noexcept;
    };

    struct stream_deleter
    {
        void operator()(void* stream) const noexcept;
    };

    std::unique_ptr<void, handle_deleter> m_handle;
    std::unique_ptr<void, stream_deleter> m_stream;
};
} // namespace lochfolk

#endif
