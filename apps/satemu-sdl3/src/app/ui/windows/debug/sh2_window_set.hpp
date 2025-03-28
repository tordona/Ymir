#pragma once

#include "sh2_cache_window.hpp"
#include "sh2_debugger_window.hpp"
#include "sh2_divu_window.hpp"
#include "sh2_dmac_window.hpp"
#include "sh2_interrupt_trace_window.hpp"
#include "sh2_interrupts_window.hpp"
#include "sh2_timers_window.hpp"

namespace app::ui {

struct SH2WindowSet {
    SH2WindowSet(SharedContext &context, bool master)
        : debugger(context, master)
        , interrupts(context, master)
        , interruptTrace(context, master)
        , cache(context, master)
        , divisionUnit(context, master)
        , timers(context, master)
        , dmaController(context, master) {}

    SH2DebuggerWindow debugger;
    SH2InterruptsWindow interrupts;
    SH2InterruptTraceWindow interruptTrace;
    SH2CacheWindow cache;
    SH2DivisionUnitWindow divisionUnit;
    SH2TimersWindow timers;
    SH2DMAControllerWindow dmaController;

    void DisplayAll() {
        debugger.Display();
        interrupts.Display();
        interruptTrace.Display();
        cache.Display();
        divisionUnit.Display();
        timers.Display();
        dmaController.Display();
    }
};

} // namespace app::ui
