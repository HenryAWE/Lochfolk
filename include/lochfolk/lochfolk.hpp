#ifndef LOCHFOLK_LOCHFOLK_HPP
#define LOCHFOLK_LOCHFOLK_HPP

#pragma once

#include <tuple>

// IWYU pragma: begin_exports

#include "detail/config.hpp"
#include "stream.hpp"
#include "archive.hpp"
#include "path.hpp"
#include "vfs.hpp"

// IWYU pragma: end_exports

#define LOCHFOLK_VERSION_MAJOR 0
#define LOCHFOLK_VERSION_MINOR 1
#define LOCHFOLK_VERSION_PATCH 0

namespace lochfolk
{
LOCHFOLK_API
std::tuple<int, int, int> get_version() noexcept;
} // namespace lochfolk

#endif
