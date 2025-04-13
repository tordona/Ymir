#pragma once

#include "peripheral_defs.hpp"
#include "peripheral_state_standard_pad.hpp"

#include <satemu/util/callback.hpp>

namespace satemu::peripheral {

// A report to be filled when a peripheral is read.
struct PeripheralReport {
    PeripheralType type;

    union Report {
        // Valid when type == PeripheralType::StandardPad
        struct StandardPad {
            StandardPadButton buttons;
        } standardPad;
    } report;
};

// Invoked when a peripheral requests a report.
using CBPeripheralReport = util::OptionalCallback<void(PeripheralReport &report)>;

} // namespace satemu::peripheral
