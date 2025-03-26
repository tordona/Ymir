#pragma once

#include <app/ui/window_base.hpp>

#include <app/ui/views/debug/sh2_divu_registers_view.hpp>
#include <app/ui/views/debug/sh2_divu_trace_view.hpp>

namespace app::ui {

class SH2DivisionUnitWindow : public WindowBase {
public:
    SH2DivisionUnitWindow(SharedContext &context, bool master);

protected:
    void PrepareWindow() override;
    void DrawContents() override;

private:
    SH2DivisionUnitRegistersView m_divuRegsView;
    SH2DivisionUnitTraceView m_divuTraceView;
};

} // namespace app::ui
