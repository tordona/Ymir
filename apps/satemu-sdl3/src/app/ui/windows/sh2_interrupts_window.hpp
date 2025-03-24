#pragma once

#include <app/shared_context.hpp>

#include <app/ui/views/sh2_interrupts_view.hpp>

namespace app::ui {

class SH2InterruptsWindow {
public:
    SH2InterruptsWindow(SharedContext &context, bool master);

    void Display();

    bool Open = false;

private:
    SharedContext &m_context;
    bool m_master;

    SH2InterruptsView m_intrView;
};

} // namespace app::ui
