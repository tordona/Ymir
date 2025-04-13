#include <satemu/core/hash.hpp>

namespace satemu {

std::string ToString(const Hash128 &hash) {
    fmt::memory_buffer buf{};
    auto inserter = std::back_inserter(buf);
    for (uint8 b : hash) {
        fmt::format_to(inserter, "{:02X}", b);
    }
    return fmt::to_string(buf);
}

} // namespace satemu
