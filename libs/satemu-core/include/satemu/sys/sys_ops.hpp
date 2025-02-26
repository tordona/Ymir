#pragma once

#include <satemu/sys/clocks.hpp>

namespace satemu::sys {

// The subset of system operations used by the SMPC.
class ISystemOperations {
public:
    virtual bool GetNMI() const = 0;
    virtual void RaiseNMI() = 0;

    virtual void EnableAndResetSlaveSH2() = 0;
    virtual void DisableSlaveSH2() = 0;

    virtual void EnableAndResetM68K() = 0;
    virtual void DisableM68K() = 0;

    virtual void SoftResetSystem() = 0;      // Soft resets the entire system
    virtual void ClockChangeSoftReset() = 0; // Soft resets VDP, SCU and SCSP after a clock change

    virtual ClockSpeed GetClockSpeed() const = 0;
    virtual void SetClockSpeed(ClockSpeed clockSpeed) = 0;
};

} // namespace satemu::sys
