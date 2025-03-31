#pragma once

#include <app/shared_context.hpp>

#include <satemu/hw/scu/scu.hpp>

namespace app::ui {

class SCURegistersView {
public:
    SCURegistersView(SharedContext &context);

    void Display();

private:
    satemu::scu::SCU &m_scu;
};

} // namespace app::ui
