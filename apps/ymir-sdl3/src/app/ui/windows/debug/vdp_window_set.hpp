#pragma once

#include "vdp2_layers_window.hpp"

namespace app::ui {

struct VDPWindowSet {
    VDPWindowSet(SharedContext &context)
        : vdp2Layers(context) {}

    void DisplayAll() {
        vdp2Layers.Display();
    }

    VDP2LayersWindow vdp2Layers;
};

} // namespace app::ui
