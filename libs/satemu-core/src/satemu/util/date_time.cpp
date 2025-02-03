#include <satemu/util/date_time.hpp>

#include <chrono>

namespace util::datetime {

DateTime host() {
    const auto localTime = std::chrono::current_zone()->to_local(std::chrono::system_clock::now());
    const std::chrono::year_month_day ymd{std::chrono::floor<std::chrono::days>(localTime)};
    const std::chrono::weekday wd{std::chrono::floor<std::chrono::days>(localTime)};
    const std::chrono::hh_mm_ss hms{localTime.time_since_epoch()};

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

} // namespace util::datetime
