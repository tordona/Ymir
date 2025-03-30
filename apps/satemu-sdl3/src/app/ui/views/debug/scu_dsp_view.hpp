#pragma once

#include <app/shared_context.hpp>

#include <satemu/hw/scu/scu_dsp.hpp>

namespace app::ui {

class SCUDSPView {
public:
    SCUDSPView(SharedContext &context);

    void Display();

private:
    SharedContext &m_context;
    /*satemu::scu::SCUDSP &m_scudsp;*/
};

} // namespace app::ui
