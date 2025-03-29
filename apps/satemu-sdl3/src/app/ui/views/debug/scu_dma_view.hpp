#pragma once

#include <app/shared_context.hpp>

#include <satemu/hw/scu/scu.hpp>

namespace app::ui {

class SCUDMAView {
public:
    SCUDMAView(SharedContext &context);

    void Display();

private:
    SharedContext &m_context;
    satemu::scu::SCU &m_scu;
};

} // namespace app::ui
