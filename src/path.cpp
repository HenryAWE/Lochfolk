#include <lochfolk/path.hpp>
#include <algorithm>
#include <iterator>

namespace lochfolk
{
bool path_view::empty() const noexcept
{
    return m_str.empty();
}

bool path_view::is_absolute() const noexcept
{
    if(m_str.empty())
        return false;

    // Check if starts with separator
    if(m_str[0] != separator)
        return false;

    // Check for . or .. components
    std::size_t start = 0;
    std::size_t end = m_str.find(separator);

    while(end != std::string_view::npos)
    {
        std::string_view component = m_str.substr(start, end - start);

        if(component == "." || component == "..")
            return false;

        start = end + 1;
        end = m_str.find(separator, start);
    }

    // Check the last component after the last separator
    std::string_view last_component = std::string_view(m_str).substr(start);
    if(last_component == "." || last_component == "..")
        return false;

    return true;
}

path_view path_view::parent_path() const noexcept
{
    if(empty())
        return path_view();

    std::string_view sv = m_str;
    if(sv == "/")
        return path_view(std::string_view("/"));
    if(sv.back() == '/')
        sv.remove_suffix(1);
    std::size_t pos = sv.rfind(separator);
    if(pos == m_str.npos)
        return path_view(m_str);
    if(pos == 0)
        return path_view(std::string_view("/"));

    return path_view(sv.substr(0, pos));
}

path_view path_view::filename() const noexcept
{
    if(empty())
        return path_view();
    if(m_str.back() == separator)
        return path_view();
    std::size_t pos = m_str.rfind(separator);
    if(pos == m_str.npos)
        return *this;

    pos += 1; // Remove the separator in the front of result.
    return path_view(m_str.substr(pos));
}

path_view path_view::extension() const noexcept
{
    auto result = filename();

    if(result.empty())
        return path_view();

    auto pos = result.m_str.rfind('.');
    if(pos == result.m_str.npos || pos == 0)
        return path_view();

    result.m_str = result.m_str.substr(pos);

    return result;
}

path::path(std::string str)
    : m_str(std::move(str)) {}

path::path(const char* cstr)
    : m_str(cstr) {}

path::path(const path_view& pv)
    : path(std::string(std::string_view(pv))) {}

path::~path() = default;

path& path::operator=(const path&) = default;
path& path::operator=(path&&) noexcept = default;

bool path::operator==(const path& rhs) const noexcept
{
    return view() == rhs.view();
}

path_view path::view() const noexcept
{
    return path_view(m_str);
}

bool path::empty() const
{
    return m_str.empty();
}

bool path::is_absolute() const
{
    return path_view(*this).is_absolute();
}

path path::parent_path() const
{
    return path_view(*this).parent_path();
}

path path::filename() const
{
    return path_view(*this).filename();
}

path path::extension() const
{
    return path_view(*this).extension();
}

path& path::append(path_view p)
{
    if(p.empty())
        return *this;
    else if(p.is_absolute())
    { // p is absolute
        *this = p;
        return *this;
    }
    else // p is relative
    {
        std::string_view sv = static_cast<std::string_view>(p);
        if(m_str.empty())
            m_str = sv;
        else if(m_str.back() == separator)
        {
            m_str += sv;
        }
        else
        {
            m_str.reserve(m_str.size() + 1 + sv.size());
            m_str += separator;
            m_str.append(std::string_view(p));
        }

        return *this;
    }
}

path& path::concat(std::string_view sv)
{
    m_str.append(sv);
    return *this;
}

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
            else // filename starts with ".", e.g. ".hidden"
            {
                result += '.';
                ch = next_ch; // the following code will copy this character
                goto ordinary;
            }

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
