#include <satemu/hw/smpc/rtc.hpp>

#include <satemu/util/dev_log.hpp>
#include <satemu/util/unreachable.hpp>

#include <satemu/sys/clocks.hpp>

#include <fstream>

namespace satemu::smpc::rtc {

namespace grp {

    // -----------------------------------------------------------------------------
    // Dev log groups

    // Hierarchy:
    //
    // base

    struct base {
        static constexpr bool enabled = true;
        static constexpr devlog::Level level = devlog::level::debug;
        static constexpr std::string_view name = "RTC";
    };

} // namespace grp

RTC::RTC() {
    m_offset = 0;

    // TODO(SMPC): RTC configuration should be saved to the configuration file
    m_mode = Mode::Host;
    // m_mode = Mode::Emulated;
    // m_hardResetStrategy = HardResetStrategy::SyncToHost;
    // m_hardResetStrategy = HardResetStrategy::ResetToFixedTime;
    m_hardResetStrategy = HardResetStrategy::Preserve;

    m_timestamp = 0;
    m_resetTimestamp = 0;

    Reset(true);
}

void RTC::Reset(bool hard) {
    if (hard) {
        switch (m_hardResetStrategy) {
        case HardResetStrategy::SyncToHost: m_timestamp = util::datetime::to_timestamp(util::datetime::host()); break;
        case HardResetStrategy::ResetToFixedTime: m_timestamp = m_resetTimestamp; break;
        case HardResetStrategy::Preserve: break;
        }
    }

    m_sysClockCount = 0;
}

void RTC::UpdateSysClock(uint64 sysClock) {
    const uint64 clockDelta = sysClock - m_sysClockCount;
    const uint64 seconds = clockDelta * m_sysClockRateNum / m_sysClockRateDen;
    if (seconds > 0) [[unlikely]] {
        m_sysClockCount += seconds * m_sysClockRateDen / m_sysClockRateNum;
        m_timestamp += seconds;
    }
}

util::datetime::DateTime RTC::GetDateTime() const {
    switch (m_mode) {
    case Mode::Host: return util::datetime::host(m_offset);
    case Mode::Emulated: return util::datetime::from_timestamp(m_timestamp);
    }
    util::unreachable();
}

void RTC::SetDateTime(const util::datetime::DateTime &dateTime) {
    switch (m_mode) {
    case Mode::Host:
        m_offset = util::datetime::delta_to_host(dateTime);
        devlog::debug<grp::base>("Setting host time offset to {} seconds", m_offset);
        break;
    case Mode::Emulated:
        m_timestamp = util::datetime::to_timestamp(dateTime);
        devlog::debug<grp::base>("Setting absolute timestamp to {} seconds", m_timestamp);
        break;
    }
}

void RTC::ReadPersistentData(std::ifstream &in) {
    sint64 offset{};
    sint64 timestamp{};
    in.read((char *)&offset, sizeof(offset));
    in.read((char *)&timestamp, sizeof(timestamp));
    if (!in) {
        return;
    }
    m_offset = offset;
    m_timestamp = timestamp;
}

void RTC::WritePersistentData(std::ofstream &out) const {
    out.write((const char *)&m_offset, sizeof(m_offset));
    out.write((const char *)&m_timestamp, sizeof(m_timestamp));
}

void RTC::UpdateClockRatios(const sys::ClockRatios &clockRatios) {
    m_sysClockRateNum = clockRatios.RTCNum;
    m_sysClockRateDen = clockRatios.RTCDen;
}

} // namespace satemu::smpc::rtc
