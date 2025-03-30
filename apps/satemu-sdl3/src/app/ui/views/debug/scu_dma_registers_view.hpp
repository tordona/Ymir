#pragma once

#include <app/shared_context.hpp>

#include <satemu/hw/scu/scu.hpp>

namespace app::ui {

class SCUDMARegistersView {
public:
    SCUDMARegistersView(SharedContext &context, uint8 channel);

    void Display();

private:
    SharedContext &m_context;
    satemu::scu::SCU &m_scu;
    const uint8 m_channel;
};

} // namespace app::ui
