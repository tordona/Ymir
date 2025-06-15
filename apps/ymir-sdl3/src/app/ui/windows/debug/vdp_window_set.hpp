#pragma once

#include "vdp2_layers_window.hpp"
#include "vdp2_nbg_cp_delay_window.hpp"

namespace app::ui {

struct VDPWindowSet {
    VDPWindowSet(SharedContext &context)
        : vdp2Layers(context)
        , vdp2NBGCPDelay(context) {}

    void DisplayAll() {
        vdp2Layers.Display();
        vdp2NBGCPDelay.Display();
    }

    VDP2LayersWindow vdp2Layers;
    VDP2NBGCharPatDelayWindow vdp2NBGCPDelay;
};

} // namespace app::ui
