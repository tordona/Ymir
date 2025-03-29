#pragma once

#include <app/ui/window_base.hpp>

#include <app/ui/views/debug/scu_debug_output_view.hpp>
#include <app/ui/views/debug/scu_interrupts_view.hpp>
#include <app/ui/views/debug/scu_timers_view.hpp>

namespace app::ui {

class SCUDebuggerWindow : public WindowBase {
public:
    SCUDebuggerWindow(SharedContext &context);

protected:
    void PrepareWindow() override;
    void DrawContents() override;

private:
    SCUInterruptsView m_intrView;
    SCUDebugOutputView m_debugOutputView;
    SCUTimersView m_timersView;
};

} // namespace app::ui
