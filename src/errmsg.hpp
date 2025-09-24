#pragma once

#include <string>
#include <filesystem>
#include <lochfolk/path.hpp>

namespace lochfolk
{
std::string stdfs_err_msg(
    std::string_view prefix,
    const std::filesystem::path& p,
    std::string_view suffix = {}
);

std::string stdfs_err_msg(
    const std::filesystem::path& p,
    std::string_view suffix = {}
);

std::string vfs_err_msg(
    std::string_view prefix,
    path_view p,
    std::string_view suffix = {}
);

std::string vfs_err_msg(
    path_view p,
    std::string_view suffix = {}
);
}
