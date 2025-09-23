#include "errmsg.hpp"

namespace lochfolk
{
std::string stdfs_err_msg(
    std::string_view prefix,
    const std::filesystem::path& p,
    std::string_view suffix
)
{
#ifdef _WIN32
    auto tmp = p.u8string();
    std::string_view p_sv(
        reinterpret_cast<const char*>(tmp.c_str()), tmp.size()
    );

#else
    std::string_view p_sv = p.native();

#endif

    std::string result;
    result.reserve(
        prefix.size() + p_sv.size() + suffix.size() + 2 /* "" */
    );

    result += prefix;
    result += '"';
    result += p_sv;
    result += '"';
    result += suffix;

    return result;
}

std::string stdfs_err_msg(
    const std::filesystem::path& p,
    std::string_view suffix
)
{
    return stdfs_err_msg(
        std::string_view(), p, suffix
    );
}

std::string vfs_err_msg(
    std::string_view prefix,
    path_view p,
    std::string_view suffix
)
{
    std::string_view p_sv(p);

    std::string result;
    result.reserve(
        prefix.size() + p_sv.size() + suffix.size() + 2 /* "" */
    );

    result += prefix;
    result += '"';
    result += p_sv;
    result += '"';
    result += suffix;

    return result;
}

std::string vfs_err_msg(
    path_view p,
    std::string_view suffix
)
{
    return vfs_err_msg(
        std::string_view(), p, suffix
    );
}
} // namespace lochfolk
