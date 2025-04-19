#pragma once

#include <app/shared_context.hpp>

#include <ymir/hw/scu/scu_dsp.hpp>

namespace app::ui {

class SCUDSPDMARegistersView {
public:
    SCUDSPDMARegistersView(SharedContext &context);

    void Display();

private:
    SharedContext &m_context;
    ymir::scu::SCUDSP &m_dsp;
};

} // namespace app::ui
