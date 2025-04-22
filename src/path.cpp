#include <lochfolk/path.hpp>
#include <algorithm>

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
        const char ch = *it;

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

            if(next_ch == '.') // "..", parent path
            {
                if(result == "/") // result is root path
                    continue;
                if(result.empty())
                {
                    result = "..";
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

        result += ch;
        ++it;
    }

    if(result.empty())
        return path(".");
    return path(std::move(result));
}
} // namespace lochfolk
