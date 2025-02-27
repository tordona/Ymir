#pragma once

#include <satemu/core/types.hpp>

#include <array>

namespace satemu::sh2 {

inline constexpr std::size_t kCacheWays = 4;
inline constexpr std::size_t kCacheEntries = 64;
inline constexpr std::size_t kCacheLineSize = 16;

struct CacheEntry {
    // Tag layout:
    //   28..10: tag
    //        2: valid bit
    // All other bits must be zero
    // This matches the address array structure
    union Tag {
        uint32 u32;
        struct {
            uint32 : 2;
            uint32 valid : 1;
            uint32 : 7;
            uint32 tagAddress : 19;
            uint32 : 3;
        };
    };
    static_assert(sizeof(Tag) == sizeof(uint32));

    alignas(16) std::array<Tag, kCacheWays> tag;
    alignas(16) std::array<std::array<uint8, kCacheLineSize>, kCacheWays> line;
};

// Stores the cache LRU update bits
struct CacheLRUUpdateBits {
    uint8 andMask;
    uint8 orMask;
};

// -----------------------------------------------------------------------------
// Registers

// addr r/w  access   init      code    name
// 092  R/W  8        00        CCR     Cache Control Register
//
//   bits   r/w  code   description
//      7   R/W  W1     Way Specification (MSB)
//      6   R/W  W0     Way Specification (LSB)
//      5   R    -      Reserved - must be zero
//      4   R/W  CP     Cache Purge (0=normal, 1=purge)
//      3   R/W  TW     Two-Way Mode (0=four-way, 1=two-way)
//      2   R/W  OD     Data Replacement Disable (0=disabled, 1=data cache not updated on miss)
//      1   R/W  ID     Instruction Replacement Disabled (same as above, but for code cache)
//      0   R/W  CE     Cache Enable (0=disable, 1=enable)
union RegCCR {
    uint8 u8;
    struct {
        uint8 CE : 1;
        uint8 ID : 1;
        uint8 OD : 1;
        uint8 TW : 1;
        uint8 CP : 1;
        uint8 _rsvd5 : 1;
        uint8 Wn : 2;
    };
};

// 0E0, 0E2, 0E4 are in INTC module above

} // namespace satemu::sh2
