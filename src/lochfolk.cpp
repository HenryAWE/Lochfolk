#include <lochfolk/lochfolk.hpp>

namespace lochfolk
{
std::tuple<int, int, int> get_version() noexcept
{
    return {
        LOCHFOLK_VERSION_MAJOR,
        LOCHFOLK_VERSION_MINOR,
        LOCHFOLK_VERSION_PATCH
    };
}
} // namespace lochfolk
