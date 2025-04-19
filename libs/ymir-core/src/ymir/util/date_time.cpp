#include <ymir/util/date_time.hpp>

#include <ctime>

namespace util::datetime {

DateTime host(sint64 offsetSeconds) {
    return from_timestamp(time(nullptr) + offsetSeconds);
}

sint64 delta_to_host(const DateTime &dateTime) {
    std::tm tm{};
    tm.tm_year = dateTime.year - 1900;
    tm.tm_mon = dateTime.month - 1;
    tm.tm_mday = dateTime.day;
    tm.tm_wday = dateTime.weekday;
    tm.tm_hour = dateTime.hour;
    tm.tm_min = dateTime.minute;
    tm.tm_sec = dateTime.second;
    return mktime(&tm) - time(nullptr);
}

DateTime from_timestamp(sint64 secondsSinceEpoch) {
    tm tm{};
#ifdef _MSC_VER
    localtime_s(&tm, &secondsSinceEpoch);
#else
    tm = *localtime(&secondsSinceEpoch);
#endif

    DateTime dt{};
    dt.year = tm.tm_year + 1900;
    dt.month = tm.tm_mon + 1;
    dt.day = tm.tm_mday;
    dt.weekday = tm.tm_wday;
    dt.hour = tm.tm_hour;
    dt.minute = tm.tm_min;
    dt.second = tm.tm_sec;
    return dt;
}

sint64 to_timestamp(const DateTime &dateTime) {
    tm tm{};
    tm.tm_year = dateTime.year - 1900;
    tm.tm_mon = dateTime.month - 1;
    tm.tm_mday = dateTime.day;
    tm.tm_wday = dateTime.weekday;
    tm.tm_hour = dateTime.hour;
    tm.tm_min = dateTime.minute;
    tm.tm_sec = dateTime.second;
    return mktime(&tm);
}

} // namespace util::datetime
