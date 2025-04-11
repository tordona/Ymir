#include "standard_pad_binds_view.hpp"

namespace app::ui {

StandardPadBindsView::StandardPadBindsView(SharedContext &context)
    : SettingsViewBase(context) {}

void StandardPadBindsView::Display(satemu::peripheral::StandardPad &pad) {
    ImGui::TextUnformatted("(placeholder for standard pad input bindings view)");
}

} // namespace app::ui
