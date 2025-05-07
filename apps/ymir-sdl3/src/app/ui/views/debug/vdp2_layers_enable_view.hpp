#pragma once

#include <app/shared_context.hpp>

#include <ymir/hw/vdp/vdp.hpp>

namespace app::ui {

class VDP2LayersEnableView {
public:
    VDP2LayersEnableView(SharedContext &context, ymir::vdp::VDP &vdp);

    void Display();

private:
    SharedContext &m_context;
    ymir::vdp::VDP &m_vdp;
};

} // namespace app::ui
