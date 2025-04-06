#pragma once

#include "rtc_defs.hpp"

#include <satemu/sys/clocks.hpp>

#include <satemu/util/date_time.hpp>

#include <satemu/core/types.hpp>

#include <iosfwd>

// -----------------------------------------------------------------------------
// Forward declarations

namespace satemu::smpc {

class SMPC;

} // namespace satemu::smpc

// -----------------------------------------------------------------------------

namespace satemu::smpc::rtc {

class RTC {
public:
    RTC();

    void Reset(bool hard);

    Mode GetMode() const {
        return m_mode;
    }
    void SetMode(Mode mode) {
        m_mode = mode;
    }

    sint64 GetHostTimeOffset() const {
        return m_offset;
    }
    void SetHostTimeOffset(sint64 offset) {
        m_offset = offset;
    }

    HardResetStrategy GetHardResetStrategy() const {
        return m_hardResetStrategy;
    }
    void SetHardResetStrategy(HardResetStrategy strategy) {
        m_hardResetStrategy = strategy;
    }

    sint64 GetResetTimestamp() const {
        return m_resetTimestamp;
    }
    void SetResetTimestamp(sint64 timestamp) {
        m_resetTimestamp = timestamp;
    }

    void UpdateSysClock(uint64 sysClock);

    util::datetime::DateTime GetDateTime() const;
    void SetDateTime(const util::datetime::DateTime &dateTime);

    // TODO: replace std iostream with custom I/O class with managed endianness
    void ReadPersistentData(std::ifstream &in);
    void WritePersistentData(std::ofstream &out) const;

private:
    friend class satemu::smpc::SMPC;
    void UpdateClockRatios(const sys::ClockRatios &clockRatios);

    Mode m_mode;
    HardResetStrategy m_hardResetStrategy;

    // RTC host mode
    sint64 m_offset; // Offset in seconds added to host time

    // RTC emulated mode
    sint64 m_timestamp;       // Current RTC timestamp in seconds since Unix epoch
    sint64 m_resetTimestamp;  // RTC timestamp to restore on hard reset when using ResetToFixedTime strategy
    uint64 m_sysClockCount;   // System clock count since last update to emulated RTC
    uint64 m_sysClockRateNum; // System clock ratio numerator
    uint64 m_sysClockRateDen; // System clock ratio denominator
};

} // namespace satemu::smpc::rtc
