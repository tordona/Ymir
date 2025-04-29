#pragma once

/**
@file
@brief Internal callback definitions used by the SMPC.
*/

#include <ymir/sys/clocks.hpp>

#include <ymir/util/callback.hpp>

namespace ymir::smpc {

/// @brief Type of callback invoked when INTBACK finishes processing to raise the SCU System Manager interrupt signal.
using CBSystemManagerInterruptCallback = util::RequiredCallback<void()>;

/// @brief The subset of system operations used by the SMPC.
class ISMPCOperations {
public:
    virtual bool GetNMI() const = 0; ///< Retrieves the NMI line state
    virtual void RaiseNMI() = 0;     ///< Raises the NMI line

    virtual void EnableAndResetSlaveSH2() = 0; ///< Enables and reset the slave SH-2
    virtual void DisableSlaveSH2() = 0;        ///< Disables the slave SH-2

    virtual void EnableAndResetM68K() = 0; ///< Enables and resets the M68K CPU
    virtual void DisableM68K() = 0;        ///< Disables the M68K CPU

    virtual void SoftResetSystem() = 0;      ///< Soft resets the entire system
    virtual void ClockChangeSoftReset() = 0; ///< Soft resets VDP, SCU and SCSP after a clock change

    virtual sys::ClockSpeed GetClockSpeed() const = 0;          ///< Retrieves the current clock speed
    virtual void SetClockSpeed(sys::ClockSpeed clockSpeed) = 0; ///< Changes the current clock speed
};

} // namespace ymir::smpc
