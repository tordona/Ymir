#pragma once

#include "scu_debugger_window.hpp"
#include "scu_dma_window.hpp"
#include "scu_dsp_window.hpp"

namespace app::ui {

struct SCUWindowSet {
    SCUWindowSet(SharedContext &context)
        : debugger(context)
        , dma(context)
        , dsp(context) {}

    void DisplayAll() {
        debugger.Display();
        dma.Display();
        dsp.Display();
    }

    SCUDebuggerWindow debugger;
    SCUDMAWindow dma;
    SCUDSPWindow dsp;
};

} // namespace app::ui
