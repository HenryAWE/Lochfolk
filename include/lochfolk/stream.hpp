#ifndef LOCHFOLK_STREAM_HPP
#define LOCHFOLK_STREAM_HPP

#pragma once

#include <memory>
#include <iostream>
#include "detail/config.hpp"

namespace lochfolk
{
class ivfstream final : public std::istream
{
    using my_base = std::istream;

public:
    ivfstream() = delete;

    LOCHFOLK_API ivfstream(ivfstream&& other) noexcept;

    LOCHFOLK_API ivfstream(std::unique_ptr<std::streambuf> buf);

    LOCHFOLK_API ~ivfstream();

    /**
     * @brief Returns true if the underlying buffer is available
     */
    [[nodiscard]]
    LOCHFOLK_API bool has_buffer() const noexcept;

private:
    std::unique_ptr<std::streambuf> m_buf;
};
} // namespace lochfolk

#endif
