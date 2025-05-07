#include "vdp2_layers_enable_view.hpp"

#include <imgui.h>

using namespace ymir;

namespace app::ui {

VDP2LayersEnableView::VDP2LayersEnableView(SharedContext &context, vdp::VDP &vdp)
    : m_context(context)
    , m_vdp(vdp) {}

void VDP2LayersEnableView::Display() {
    auto checkbox = [&](const char *name, vdp::VDP::Layer layer) {
        bool enabled = m_vdp.IsLayerEnabled(layer);
        if (ImGui::Checkbox(name, &enabled)) {
            m_vdp.SetLayerEnabled(layer, enabled);
        }
    };
    checkbox("Sprite", vdp::VDP::Layer::Sprite);
    checkbox("RBG0", vdp::VDP::Layer::RBG0);
    checkbox("NBG0/RBG1", vdp::VDP::Layer::NBG0_RBG1);
    checkbox("NBG1/EXBG", vdp::VDP::Layer::NBG1_EXBG);
    checkbox("NBG2", vdp::VDP::Layer::NBG2);
    checkbox("NBG3", vdp::VDP::Layer::NBG3);
}

} // namespace app::ui
