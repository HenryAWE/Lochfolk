#include <lochfolk/path.hpp>
#include <iostream>
#include <algorithm>
#include <iterator>

namespace lochfolk
{
path_view::const_iterator::const_iterator(std::size_t pos, std::size_t len, path_view pv) noexcept
    : m_pos(pos), m_len(len), m_sv(pv) {}

bool path_view::const_iterator::operator==(const const_iterator& rhs) const noexcept
{
    return m_sv.data() == rhs.m_sv.data() &&
           m_pos == rhs.m_pos;
}

path_view path_view::const_iterator::operator*() const
{
    if(m_sv.empty() || m_pos >= m_sv.size())
        return path_view();

    std::string_view result_sv = m_sv.substr(m_pos, m_len);
    return path_view(result_sv);
}

void path_view::const_iterator::next()
{
    m_pos = m_pos + m_len;
    m_pos = m_sv.find_first_not_of(separator, m_pos);
    if(m_pos == m_sv.npos)
    {
        m_pos = m_sv.size();
        m_len = 0;
        return;
    }

    std::size_t sep_pos = m_sv.find(separator, m_pos);
    m_len = sep_pos == m_sv.npos ?
                m_sv.size() - m_pos :
                sep_pos - m_pos;
}

void path_view::const_iterator::prev()
{
    if(m_pos == 0) [[unlikely]]
    {
        m_len = 0;
        return;
    };

    std::size_t sep_pos = [&]() -> std::size_t
    {
        std::size_t i = m_pos;
        for(; i > 0; --i)
        {
            if(m_sv[i - 1] != separator)
                return i;
        }

        return i;
    }();

    if(sep_pos == 0) // Special case for the leading separator
    {
        m_pos = 0;
        m_len = 1;
        return;
    }

    std::size_t new_pos = m_sv.rfind(separator, sep_pos - 1); // Subtract 1 to skip the separator

    m_pos = new_pos == m_sv.npos ? 0 : new_pos + 1;
    m_len = sep_pos - m_pos;
}

path_view::const_iterator path_view::cbegin() const
{
    if(empty())
        return cend();

    // The first component of an absolute path is the root directory itself.
    // Do not use is_absolute() here: it is a syntactic query, but the layout
    // of the components depends only on the leading separator.
    if(m_str.front() == separator)
        return const_iterator(0, 1, *this);

    std::size_t len = m_str.find(separator);
    return const_iterator(0, len, *this);
}

path_view::const_iterator path_view::cend() const
{
    return const_iterator(m_str.size(), 0, *this);
}

bool path_view::empty() const noexcept
{
    return m_str.empty();
}

bool path_view::is_absolute() const noexcept
{
    // POSIX-style path format: a path is absolute if and only if it begins
    // with the root directory separator. "." and ".." are ordinary pathname
    // components for this query; resolving them is the job of
    // lexically_normal() and of the path consumer (see std::filesystem).
    return !m_str.empty() && m_str.front() == separator;
}

path_view path_view::parent_path() const noexcept
{
    if(empty())
        return path_view();

    std::string_view sv = m_str;
    while(sv.size() > 1 && sv.back() == separator)
        sv.remove_suffix(1);

    if(sv == "/")
        return path_view(std::string_view("/"));

    std::size_t pos = sv.rfind(separator);
    if(pos == std::string_view::npos)
        return path_view(sv);
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

std::ostream& operator<<(std::ostream& os, const path_view& pv)
{
    // Similar to operator<< for std::filesystem::path
    std::string_view sv(pv);
    if(sv.empty())
        return os;
    os << '"' << sv << '"';
    return os;
}

path::path() noexcept = default;
path::path(const path&) = default;
path::path(path&&) noexcept = default;

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

path::const_iterator::const_iterator(const underlying_type& other) noexcept
    : m_base(other) {}

path::const_iterator path::cbegin() const
{
    return const_iterator(view().cbegin());
}

path::const_iterator path::cend() const
{
    return const_iterator(view().cend());
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

    // The root directory is always kept; a relative path must stay relative.
    if(m_str.front() == separator)
        result += separator;

    const auto sentinel = m_str.end();
    for(auto it = m_str.begin(); it != sentinel;)
    {
        char ch = *it;

        if(ch == separator)
        {
            // Collapse runs of separators. Append a separator only when it
            // actually separates two components, so that a cancelled component
            // ("a/..//b") cannot turn the result into an absolute path.
            it = std::find_if(
                std::next(it),
                sentinel,
                [](char c) -> bool
                { return c != separator; }
            );

            if(!result.empty() && result.back() != separator)
                result += separator;
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
                {
                    // Locate the last component, ignoring trailing separators.
                    auto comp_end = result.size();
                    while(comp_end > 0 && result[comp_end - 1] == separator)
                        --comp_end;

                    std::size_t comp_start = 0;
                    if(comp_end > 0)
                    {
                        auto pos = result.rfind(separator, comp_end - 1);
                        comp_start = pos == result.npos ? 0 : pos + 1;
                    }

                    const std::string_view last_component(
                        result.data() + comp_start,
                        comp_end - comp_start
                    );

                    if(last_component == "..")
                    {
                        // An unresolved leading ".." must not cancel another
                        // one, e.g. "../.." stays "../..".
                        result.erase(comp_end);
                        result += separator;
                        result += "..";
                    }
                    else if(comp_end == 0)
                    {
                        // Nothing to cancel: keep a relative "..", while ".."
                        // applied to the root stays at the root.
                        if(result.empty())
                            result = "..";
                    }
                    else
                    {
                        // Cancel the last component and keep the separator in
                        // front of it: "a/b/.." -> "a/".
                        auto pos = result.rfind(separator, comp_end - 1);
                        if(pos == result.npos)
                            result.clear();
                        else
                            result.erase(pos + 1);
                    }
                }

                ++it; // Skip the second '.'
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

    // A trailing separator is kept only after a regular component or the root
    // directory, never after "." or "..".
    auto last_end = result.size();
    while(last_end > 0 && result[last_end - 1] == separator)
        --last_end;

    if(last_end != 0 && last_end < result.size())
    {
        auto pos = result.rfind(separator, last_end - 1);
        std::size_t comp_start = pos == result.npos ? 0 : pos + 1;
        const std::string_view last_component(
            result.data() + comp_start,
            last_end - comp_start
        );
        if(last_component == ".." || last_component == ".")
            result.erase(last_end);
    }

    return path(std::move(result));
}

std::ostream& operator<<(std::ostream& os, const path& p)
{
    os << p.view();
    return os;
}
} // namespace lochfolk
