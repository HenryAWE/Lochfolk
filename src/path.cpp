#include <lochfolk/path.hpp>
#include <algorithm>
#include <iterator>

namespace lochfolk
{
path path::lexically_normal() const
{
    if(empty())
        return path();

    std::string result;

    const auto sentinel = m_str.end();
    for(auto it = m_str.begin(); it != sentinel;)
    {
        char ch = *it;

        if(ch == separator)
        {
            result += separator;

            it = std::find_if(
                std::next(it),
                sentinel,
                [](char c) -> bool
                { return c != separator; }
            );
            continue;
        }

        if(ch == '.')
        {
            ++it;
            if(it == sentinel)
                break;

            char next_ch = *it;

            if(next_ch == '.')
            {
                if(auto peek_it = std::next(it); peek_it != sentinel && *peek_it != separator)
                {
                    result += '.';
                    it = peek_it;
                    goto ordinary; // Ordinary path starts with "..", e.g. "..file"
                }

                // "..", parent path

                if(result == "/") // result is root path
                {
                    ++it;
                    continue;
                }
                else if(result.empty())
                {
                    result = "..";
                    ++it;
                    continue;
                }

                if(result.back() == separator)
                    result.pop_back();
                auto pos = result.rfind(separator);
                if(pos == result.npos)
                    result.clear();
                else
                    result.erase(pos + 1);

                continue;
            }

            if(next_ch == '/') // "./", current path
                ++it;

            continue;
        }

ordinary:
        result += ch;
        ++it;

        auto stop = std::find(it, sentinel, separator);
        std::copy(it, stop, std::back_inserter(result));
        it = stop;
    }

    if(result.empty())
        return path(".");
    return path(std::move(result));
}
} // namespace lochfolk
