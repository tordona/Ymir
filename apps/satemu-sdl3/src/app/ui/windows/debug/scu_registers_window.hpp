#pragma once

#include <app/ui/window_base.hpp>

#include <app/ui/views/debug/scu_interrupt_trace_view.hpp>
#include <app/ui/views/debug/scu_interrupts_view.hpp>
#include <app/ui/views/debug/scu_registers_view.hpp>
#include <app/ui/views/debug/scu_timers_view.hpp>

namespace app::ui {

class SCURegistersWindow : public WindowBase {
public:
    SCURegistersWindow(SharedContext &context);

protected:
    void PrepareWindow() override;
    void DrawContents() override;

private:
    SCURegistersView m_regsView;
    SCUInterruptsView m_intrView;
    SCUTimersView m_timersView;
    SCUInterruptTraceView m_intrTraceView;
};

} // namespace app::ui
