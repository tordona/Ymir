#pragma once

#include "scsp_slots_window.hpp"

namespace app::ui {

struct SCSPWindowSet {
    SCSPWindowSet(SharedContext &context)
        : slots(context) {}

    void DisplayAll() {
        slots.Display();
    }

    SCSPSlotsWindow slots;
};

} // namespace app::ui
