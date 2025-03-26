#pragma once

#include <app/ui/window_base.hpp>

#include <app/ui/views/debug/sh2_interrupts_view.hpp>

namespace app::ui {

class SH2InterruptsWindow : public WindowBase {
public:
    SH2InterruptsWindow(SharedContext &context, bool master);

protected:
    void DrawContents() override;

private:
    SH2InterruptsView m_intrView;
};

} // namespace app::ui
