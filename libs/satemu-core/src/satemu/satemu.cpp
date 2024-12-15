#include <satemu/export.h>
#include <satemu/satemu.hpp>

#include <bit>

static_assert(std::endian::native == std::endian::little, "big-endian platforms are not supported at this moment");
static_assert(CHAR_BIT == 8, "char is expected to have 8 bits");

namespace satemu {

// LIB_EXPORT void func() {}

} // namespace satemu
