#pragma once

#include "scsp_output_window.hpp"
#include "scsp_slots_window.hpp"

namespace app::ui {

struct SCSPWindowSet {
    SCSPWindowSet(SharedContext &context)
        : output(context)
        , slots(context) {}

    void DisplayAll() {
        output.Display();
        slots.Display();
    }

    SCSPOutputWindow output;
    SCSPSlotsWindow slots;
};

} // namespace app::ui
