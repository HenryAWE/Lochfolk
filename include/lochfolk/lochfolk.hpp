#ifndef LOCHFOLK_LOCHFOLK_HPP
#define LOCHFOLK_LOCHFOLK_HPP

#pragma once

#include <tuple>
#include <utility>
#include <filesystem>
#include "config.hpp" // IWYU pragma: exports

#define LOCHFOLK_VERSION_MAJOR 0
#define LOCHFOLK_VERSION_MINOR 1
#define LOCHFOLK_VERSION_PATCH 0

namespace lochfolk
{
std::tuple<int, int, int> get_version() noexcept;
} // namespace lochfolk

#endif
