#pragma once

#include <app/shared_context.hpp>

#include <satemu/hw/scu/scu.hpp>

namespace app::ui {

class SCUInterruptsView {
public:
    SCUInterruptsView(SharedContext &context);

    void Display();

private:
    SharedContext &m_context;
    satemu::scu::SCU &m_scu;

    void DisplayInternalInterrupts();
    void DisplayExternalInterrupts();
};

} // namespace app::ui
