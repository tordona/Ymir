#pragma once

#include <ymir/core/types.hpp>

namespace util::datetime {

// Represents a date and time in the Gregorian calendar.
struct DateTime {
    sint32 year;   // The current year; negative is B.C., positive is A.D.
    uint8 month;   // The current month: 1 (January) to 12 (December)
    uint8 day;     // The current day of the month: 1 to 31
    uint8 weekday; // The current weekday: 0 (Sunday) to 6 (Saturday)
    uint8 hour;    // The current hour of the day 0 to 23
    uint8 minute;  // The current minute of the hour: 0 to 59
    uint8 second;  // The current second of the minute: 0 to 59
};

// Gets the current host (system) date and time with the specified offset in seconds.
DateTime host(sint64 offsetSeconds = 0);

// Computes the number of seconds between the given dateTime and the current host time.
sint64 delta_to_host(const DateTime &dateTime);

// Builds a DateTime from the given amount of seconds since Unix epoch (1970/01/01 00:00:00).
DateTime from_timestamp(sint64 secondsSinceEpoch);

// Converts a DateTime into an Unix timestamp with granularity in seconds.
sint64 to_timestamp(const DateTime &dateTime);

} // namespace util::datetime
