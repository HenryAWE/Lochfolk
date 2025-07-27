#ifndef LOCHFOLK_STREAM_HPP
#define LOCHFOLK_STREAM_HPP

#pragma once

#include <memory>
#include <iostream>

namespace lochfolk
{
class ivfstream final : public std::istream
{
    using my_base = std::istream;

public:
    ivfstream();

    ivfstream(ivfstream&& other) noexcept;

    ivfstream(std::unique_ptr<std::streambuf> buf);

    ~ivfstream();

    /**
     * @brief Returns true if the underlying buffer is available
     */
    bool has_buffer() const noexcept;

private:
    std::unique_ptr<std::streambuf> m_buf;
};
} // namespace lochfolk

#endif
