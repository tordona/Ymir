#pragma once

#include <app/shared_context.hpp>

#include <app/ui/views/sh2_interrupt_tracer_view.hpp>

namespace app::ui {

class SH2InterruptTracerWindow {
public:
    SH2InterruptTracerWindow(SharedContext &context, bool master);

    void Display();

    bool Open = false;

private:
    SharedContext &m_context;
    bool m_master;

    SH2InterruptTracerView m_intrTracerView;
};

} // namespace app::ui
