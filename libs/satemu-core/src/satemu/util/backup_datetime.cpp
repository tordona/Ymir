#include <satemu/util/backup_datetime.hpp>

#include <chrono>

namespace util {

static const auto kOrigin = [] {
    std::tm t{};
    t.tm_year = 1980 - 1900;
    t.tm_mon = 1 - 1;
    t.tm_mday = 1;
    t.tm_wday = 2;
    t.tm_hour = 0;
    t.tm_min = 0;
    t.tm_sec = 0;
    return std::chrono::current_zone()->to_local(std::chrono::system_clock::from_time_t(std::mktime(&t)));
}();

BackupDateTime::BackupDateTime(uint32 raw) {
    const auto date = kOrigin + std::chrono::minutes(raw);

    const std::chrono::year_month_day ymd{std::chrono::floor<std::chrono::days>(date)};
    const std::chrono::weekday wd{std::chrono::floor<std::chrono::days>(date)};
    const std::chrono::hh_mm_ss hms{date.time_since_epoch()};

    year = static_cast<int>(ymd.year());
    month = static_cast<unsigned int>(ymd.month());
    day = static_cast<unsigned int>(ymd.day());
    hour = hms.hours().count() % 24;
    minute = hms.minutes().count();
}

uint32 BackupDateTime::ToRaw() const {
    std::tm t{};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = minute;
    t.tm_sec = 0;
    const auto givenTime =
        std::chrono::current_zone()->to_local(std::chrono::system_clock::from_time_t(std::mktime(&t)));
    return std::chrono::duration_cast<std::chrono::minutes>(givenTime - kOrigin).count();
}

} // namespace util
