#ifndef LOCHFOLK_STREAM_HPP
#define LOCHFOLK_STREAM_HPP

#pragma once

#include <memory>
#include <iostream>

namespace lochfolk
{
class ivfstream : public std::istream
{
    using my_base = std::istream;

public:
    ivfstream() = delete;

    ivfstream(ivfstream&& other) noexcept;

    ivfstream(std::unique_ptr<std::streambuf> buf);

    ~ivfstream();
};
} // namespace lochfolk

#endif
