#include <satemu/hw/smpc/rtc.hpp>

#include <satemu/util/debug_print.hpp>
#include <satemu/util/unreachable.hpp>

namespace satemu::smpc::rtc {

static constexpr dbg::Category rootLog{"RTC"};

RTC::RTC() {
    // TODO(SMPC): RTC offset should be persisted
    m_offset = 0;

    // TODO(SMPC): RTC configuration should be saved to the configuration file
    m_mode = Mode::Host;
    // m_mode = Mode::Emulated;
    // m_hardResetStrategy = HardResetStrategy::SyncToHost;
    // m_hardResetStrategy = HardResetStrategy::ResetToFixedTime;
    m_hardResetStrategy = HardResetStrategy::Preserve;

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

void RTC::SetSysClockRate(uint64 sysClockRate) {
    m_sysClockRate = sysClockRate;
}

void RTC::UpdateSysClock(uint64 sysClock) {
    const uint64 clockDelta = sysClock - m_sysClockCount;
    const uint64 seconds = clockDelta / m_sysClockRate;
    m_sysClockCount += m_sysClockRate * seconds;
    m_timestamp += seconds;
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
        rootLog.debug("Setting host time offset to {} seconds", m_offset);
        break;
    case Mode::Emulated:
        m_timestamp = util::datetime::to_timestamp(dateTime);
        rootLog.debug("Setting absolute timestamp to {} seconds", m_timestamp);
        break;
    }
}

} // namespace satemu::smpc::rtc
