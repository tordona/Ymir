#pragma once

#include <satemu/core/hash.hpp>

namespace satemu::db {

enum class SystemVariant { None, SaturnJP, SaturnUS_EU, HiSaturn, GameNaviHiSaturn, SamsungSaturn, VSaturn, DevKit };

struct IPLROMInfo {
    const char *commonFilename;
    const char *version;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    SystemVariant sysVariant;
};

// Retrieves information about an IPL ROM image given its XXH128 hash.
// Returns nullptr if there is no information for the given hash.
const IPLROMInfo *GetIPLROMInfo(Hash128 hash);

} // namespace satemu::db
