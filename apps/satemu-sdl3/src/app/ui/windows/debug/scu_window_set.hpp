#pragma once

#include "scu_dma_window.hpp"
#include "scu_dsp_window.hpp"
#include "scu_registers_window.hpp"

namespace app::ui {

struct SCUWindowSet {
    SCUWindowSet(SharedContext &context)
        : regs(context)
        , dma(context)
        , dsp(context) {}

    void DisplayAll() {
        regs.Display();
        dma.Display();
        dsp.Display();
    }

    SCURegistersWindow regs;
    SCUDMAWindow dma;
    SCUDSPWindow dsp;
};

} // namespace app::ui
