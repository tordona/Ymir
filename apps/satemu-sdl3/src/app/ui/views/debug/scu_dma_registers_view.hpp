#pragma once

#include <app/shared_context.hpp>

#include <satemu/hw/scu/scu.hpp>

namespace app::ui {

class SCUDMARegistersView {
public:
    SCUDMARegistersView(SharedContext &context);

    void Display(uint8 channel);

private:
    SharedContext &m_context;
    satemu::scu::SCU &m_scu;
};

} // namespace app::ui
