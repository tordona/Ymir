#pragma once

#include "cdblock_cmd_trace_window.hpp"
#include "cdblock_filters_window.hpp"

namespace app::ui {

struct CDBlockWindowSet {
    CDBlockWindowSet(SharedContext &context)
        : cmdTrace(context)
        , filters(context) {}

    void DisplayAll() {
        cmdTrace.Display();
        filters.Display();
    }

    CDBlockCommandTraceWindow cmdTrace;
    CDBlockFiltersWindow filters;
};

} // namespace app::ui
