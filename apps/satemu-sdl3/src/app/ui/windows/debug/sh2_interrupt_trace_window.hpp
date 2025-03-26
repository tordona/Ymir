#pragma once

#include <app/ui/window_base.hpp>

#include <app/ui/views/debug/sh2_interrupt_trace_view.hpp>

namespace app::ui {

class SH2InterruptTraceWindow : public WindowBase {
public:
    SH2InterruptTraceWindow(SharedContext &context, bool master);

protected:
    void PrepareWindow() override;
    void DrawContents() override;

private:
    SH2InterruptTraceView m_intrTraceView;
};

} // namespace app::ui
