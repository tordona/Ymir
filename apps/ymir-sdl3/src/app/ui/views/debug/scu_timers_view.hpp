#pragma once

#include <app/shared_context.hpp>

#include <ymir/hw/scu/scu.hpp>

namespace app::ui {

class SCUTimersView {
public:
    SCUTimersView(SharedContext &context);

    void Display();

private:
    SharedContext &m_context;
    ymir::scu::SCU &m_scu;
};

} // namespace app::ui
