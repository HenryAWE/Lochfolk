#ifndef LOCHFOLK_ZIP_ARCHIVE_HPP
#define LOCHFOLK_ZIP_ARCHIVE_HPP

#pragma once

#include <cassert>
#include <span>
#include <format>
#include <filesystem>
#include <minizip/mz.h>
#include <minizip/mz_zip.h>
#include <minizip/mz_strm_os.h>
#include <lochfolk/archive.hpp>

namespace lochfolk
{
class zip_archive : public archive
{
public:
    class minizip_error : public std::runtime_error
    {
    public:
        minizip_error(std::int32_t err)
            : runtime_error(translate_error(err)) {}

        static std::string translate_error(std::int32_t err)
        {
#define LOCHFOLK_TRANSLATE_MZ_ERROR(name) \
case name: return #name

            switch(err)
            {
                LOCHFOLK_TRANSLATE_MZ_ERROR(MZ_OK);
                LOCHFOLK_TRANSLATE_MZ_ERROR(MZ_STREAM_ERROR);
                LOCHFOLK_TRANSLATE_MZ_ERROR(MZ_DATA_ERROR);
                LOCHFOLK_TRANSLATE_MZ_ERROR(MZ_MEM_ERROR);
                LOCHFOLK_TRANSLATE_MZ_ERROR(MZ_BUF_ERROR);
                LOCHFOLK_TRANSLATE_MZ_ERROR(MZ_VERSION_ERROR);
                LOCHFOLK_TRANSLATE_MZ_ERROR(MZ_END_OF_LIST);
                LOCHFOLK_TRANSLATE_MZ_ERROR(MZ_END_OF_STREAM);
                LOCHFOLK_TRANSLATE_MZ_ERROR(MZ_PARAM_ERROR);
                LOCHFOLK_TRANSLATE_MZ_ERROR(MZ_FORMAT_ERROR);
                LOCHFOLK_TRANSLATE_MZ_ERROR(MZ_INTERNAL_ERROR);
                LOCHFOLK_TRANSLATE_MZ_ERROR(MZ_CRC_ERROR);
                LOCHFOLK_TRANSLATE_MZ_ERROR(MZ_CRYPT_ERROR);
                LOCHFOLK_TRANSLATE_MZ_ERROR(MZ_EXIST_ERROR);
                LOCHFOLK_TRANSLATE_MZ_ERROR(MZ_PASSWORD_ERROR);
                LOCHFOLK_TRANSLATE_MZ_ERROR(MZ_SUPPORT_ERROR);
                LOCHFOLK_TRANSLATE_MZ_ERROR(MZ_HASH_ERROR);
                LOCHFOLK_TRANSLATE_MZ_ERROR(MZ_OPEN_ERROR);
                LOCHFOLK_TRANSLATE_MZ_ERROR(MZ_CLOSE_ERROR);
                LOCHFOLK_TRANSLATE_MZ_ERROR(MZ_SEEK_ERROR);
                LOCHFOLK_TRANSLATE_MZ_ERROR(MZ_TELL_ERROR);
                LOCHFOLK_TRANSLATE_MZ_ERROR(MZ_READ_ERROR);
                LOCHFOLK_TRANSLATE_MZ_ERROR(MZ_WRITE_ERROR);
                LOCHFOLK_TRANSLATE_MZ_ERROR(MZ_SIGN_ERROR);
                LOCHFOLK_TRANSLATE_MZ_ERROR(MZ_SYMLINK_ERROR);

            default:
                return std::format("MZ_ERROR({})", err);
            }

#undef LOCHFOLK_TRANSLATE_MZ_ERROR
        }
    };

    zip_archive()
        : m_handle(mz_zip_create()),
          m_stream(mz_stream_os_create())
    {}

    zip_archive(const zip_archive&) = delete;

    ~zip_archive()
    {
        close();
        mz_zip_delete(&m_handle);
        m_stream.reset();
    }

    std::unique_ptr<std::streambuf> getbuf(std::int64_t offset, std::ios_base::openmode mode) override
    {
        goto_entry(offset);
        mode &= ~std::ios_base::out;
        return get_entry_buf(mode);
    }

    void open(const std::filesystem::path& sys_path)
    {
        std::int32_t err = mz_stream_os_open(
            m_stream.get(),
            reinterpret_cast<const char*>(sys_path.u8string().c_str()),
            MZ_OPEN_MODE_READ
        );
        if(err != MZ_OK)
            throw minizip_error(err);

        err = mz_zip_open(m_handle, m_stream.get(), MZ_OPEN_MODE_READ);
        if(err != MZ_OK)
            throw minizip_error(err);
    }

    void close()
    {
        mz_zip_close(m_handle);
        mz_stream_os_close(m_stream.get());
    }

    bool goto_first()
    {
        std::int32_t err = mz_zip_goto_first_entry(m_handle);
        if(err == MZ_END_OF_LIST)
            return false;
        else if(err == MZ_OK)
            return true;
        else
            throw minizip_error(err);
    }

    /**
     * @brief Goto next entry
     *
     * @return true No error
     * @return false End of file list
     */
    bool goto_next()
    {
        std::int32_t err = mz_zip_goto_next_entry(m_handle);
        if(err == MZ_END_OF_LIST)
            return false;
        else if(err == MZ_OK)
            return true;
        else
            throw minizip_error(err);
    }

    void goto_entry(std::int64_t offset)
    {
        int err = mz_zip_goto_entry(m_handle, offset);
        if(err != MZ_OK)
            throw minizip_error(err);
    }

    bool entry_is_dir() const
    {
        return mz_zip_entry_is_dir(m_handle) == MZ_OK;
    }

    std::int64_t get_entry_offset() const
    {
        return mz_zip_get_entry(m_handle);
    }

    const mz_zip_file& get_entry_info()
    {
        mz_zip_file* ptr = nullptr;
        int err = mz_zip_entry_get_info(m_handle, &ptr);
        if(err != MZ_OK)
            throw minizip_error(err);
        assert(ptr != nullptr);
        return *ptr;
    }

    std::unique_ptr<std::streambuf> get_entry_buf(std::ios_base::openmode mode)
    {
        std::string result;

        open_entry();
        try
        {
            while(true)
            {
                std::byte buf[512];
                std::size_t len = read_entry(buf);
                if(len == 0)
                    break;

                result.append(reinterpret_cast<char*>(buf), len);
            }
            close_entry();
        }
        catch(...)
        {
            close_entry();
            throw;
        }

        return std::make_unique<std::stringbuf>(std::move(result), mode);
    }

    void open_entry()
    {
        int err = mz_zip_entry_read_open(m_handle, false, nullptr);
        if(err != MZ_OK)
            throw minizip_error(err);
    }

    std::size_t read_entry(std::span<std::byte> buf)
    {
        std::int32_t result = mz_zip_entry_read(
            m_handle, buf.data(), static_cast<std::int32_t>(buf.size())
        );
        if(result < 0)
            throw minizip_error(result);

        return static_cast<size_t>(result);
    }

    void close_entry() noexcept
    {
        mz_zip_entry_close(m_handle);
    }

private:
    struct stream_deleter
    {
        void operator()(void* stream) const noexcept
        {
            if(!stream) [[unlikely]]
                return;
            mz_stream_os_delete(&stream);
        }
    };

    void* m_handle;
    std::unique_ptr<void, stream_deleter> m_stream;
};
} // namespace lochfolk

#endif
