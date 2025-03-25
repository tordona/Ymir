#pragma once

#include <app/shared_context.hpp>

#include <app/ui/views/sh2_divu_registers_view.hpp>
#include <app/ui/views/sh2_divu_trace_view.hpp>

namespace app::ui {

class SH2DivisionUnitWindow {
public:
    SH2DivisionUnitWindow(SharedContext &context, bool master);

    void Display();

    bool Open = false;

private:
    SharedContext &m_context;
    bool m_master;

    SH2DivisionUnitRegistersView m_divuRegsView;
    SH2DivisionUnitTraceView m_divuTraceView;
};

} // namespace app::ui
