#pragma once

#include <app/shared_context.hpp>

#include <app/ui/views/sh2_interrupt_trace_view.hpp>

namespace app::ui {

class SH2InterruptTraceWindow {
public:
    SH2InterruptTraceWindow(SharedContext &context, bool master);

    void Display();

    bool Open = false;

private:
    SharedContext &m_context;
    bool m_master;

    SH2InterruptTraceView m_intrTraceView;
};

} // namespace app::ui
