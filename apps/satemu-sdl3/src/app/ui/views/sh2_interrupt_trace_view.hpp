#pragma once

#include <app/shared_context.hpp>

#include <app/debug/sh2_tracer.hpp>

#include <satemu/hw/sh2/sh2.hpp>

namespace app::ui {

class SH2InterruptTraceView {
public:
    SH2InterruptTraceView(SharedContext &context, satemu::sh2::SH2 &sh2, SH2Tracer &tracer);

    void Display();

private:
    SharedContext &m_context;
    satemu::sh2::SH2 &m_sh2;
    SH2Tracer &m_tracer;
};

} // namespace app::ui
