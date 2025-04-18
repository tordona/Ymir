#pragma once

#include <satemu/core/hash.hpp>

namespace satemu::db {

enum class SystemVariant { None, Saturn, HiSaturn, VSaturn, DevKit };
enum class SystemRegion { None, US_EU, JP, KR, RegionFree };

struct IPLROMInfo {
    const char *version;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    SystemVariant variant;
    SystemRegion region;
};

// Retrieves information about an IPL ROM image given its XXH128 hash.
// Returns nullptr if there is no information for the given hash.
const IPLROMInfo *GetIPLROMInfo(XXH128Hash hash);

} // namespace satemu::db
