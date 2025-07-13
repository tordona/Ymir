#pragma once

#include "vdp1_registers_window.hpp"
#include "vdp2_layer_visibility_window.hpp"
#include "vdp2_nbg_cp_delay_window.hpp"

namespace app::ui {

struct VDPWindowSet {
    VDPWindowSet(SharedContext &context)
        : vdp1Regs(context)
        , vdp2LayerVisibility(context)
        , vdp2NBGCPDelay(context) {}

    void DisplayAll() {
        vdp1Regs.Display();
        vdp2LayerVisibility.Display();
        vdp2NBGCPDelay.Display();
    }

    VDP1RegistersWindow vdp1Regs;
    VDP2LayerVisibilityWindow vdp2LayerVisibility;
    VDP2NBGCharPatDelayWindow vdp2NBGCPDelay;
};

} // namespace app::ui
