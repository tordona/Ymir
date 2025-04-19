#pragma once

#include <ymir/core/types.hpp>

namespace util {

// Converts from and to the raw backup date/time format (minutes since 01/01/1980)
struct BackupDateTime {
    BackupDateTime(uint32 raw);

    uint32 year;
    uint32 month;
    uint32 day;
    uint32 hour;
    uint32 minute;

    uint32 ToRaw() const;
};

} // namespace util