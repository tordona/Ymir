#pragma once

#include <app/shared_context.hpp>

#include "views/sh2_interrupt_tracer_view.hpp"

namespace app {

class SH2InterruptTracer {
public:
    SH2InterruptTracer(SharedContext &context, bool master);

    void Display();

    bool Open = false;

private:
    SharedContext &m_context;
    bool m_master;

    SH2InterruptTracerView m_intrTracerView;
};

} // namespace app
