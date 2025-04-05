#ifndef LOCHFOLK_ARCHIVE_HPP
#define LOCHFOLK_ARCHIVE_HPP

#pragma once

#include <iostream>

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
    ) = 0;
};

class ivfstream : public std::istream
{
    using my_base = std::istream;

public:
    ivfstream(std::unique_ptr<std::streambuf> buf);

    ~ivfstream();
};
} // namespace lochfolk

#endif
