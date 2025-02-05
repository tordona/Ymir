#include <satemu/util/date_time.hpp>

#include <chrono>

namespace util::datetime {

template <typename T>
static DateTime MakeDateTime(T tp) {
    const std::chrono::year_month_day ymd{std::chrono::floor<std::chrono::days>(tp)};
    const std::chrono::weekday wd{std::chrono::floor<std::chrono::days>(tp)};
    const std::chrono::hh_mm_ss hms{tp.time_since_epoch()};

    DateTime dt{};
    dt.year = static_cast<int>(ymd.year());
    dt.month = static_cast<unsigned int>(ymd.month());
    dt.day = static_cast<unsigned int>(ymd.day());
    dt.weekday = wd.c_encoding();
    dt.hour = hms.hours().count() % 24;
    dt.minute = hms.minutes().count();
    dt.second = hms.seconds().count();
    return dt;
}

DateTime host(sint64 offsetSeconds) {
    const std::chrono::seconds offset{offsetSeconds};
    return MakeDateTime(std::chrono::current_zone()->to_local(std::chrono::system_clock::now() + offset));
}

sint64 delta_to_host(const DateTime &dateTime) {
    std::tm t{};
    t.tm_year = dateTime.year - 1900;
    t.tm_mon = dateTime.month - 1;
    t.tm_mday = dateTime.day;
    t.tm_wday = dateTime.weekday;
    t.tm_hour = dateTime.hour;
    t.tm_min = dateTime.minute;
    t.tm_sec = dateTime.second;
    const auto givenTime = std::chrono::system_clock::from_time_t(std::mktime(&t));
    const auto delta = givenTime - std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(delta).count();
}

DateTime from_timestamp(sint64 secondsSinceEpoch) {
    const auto time = std::chrono::system_clock::from_time_t(secondsSinceEpoch);
    return MakeDateTime(time);
}

sint64 to_timestamp(const DateTime &dateTime) {
    std::tm t{};
    t.tm_year = dateTime.year - 1900;
    t.tm_mon = dateTime.month - 1;
    t.tm_mday = dateTime.day;
    t.tm_wday = dateTime.weekday;
    t.tm_hour = dateTime.hour;
    t.tm_min = dateTime.minute;
    t.tm_sec = dateTime.second;
    const auto givenTime =
        std::chrono::current_zone()->to_local(std::chrono::system_clock::from_time_t(std::mktime(&t)));
    return std::chrono::duration_cast<std::chrono::seconds>(givenTime.time_since_epoch()).count();
}

} // namespace util::datetime
