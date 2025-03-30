#pragma once

#include <app/shared_context.hpp>

#include <satemu/hw/scu/scu_dsp.hpp>

namespace app::ui {

class SCUDSPDataRAMView {
public:
    SCUDSPDataRAMView(SharedContext &context);

    void Display();

private:
    SharedContext &m_context;
    satemu::scu::SCUDSP &m_dsp;
};

} // namespace app::ui
