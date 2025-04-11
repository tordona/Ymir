#pragma once

#include <satemu/hw/smpc/peripheral/peripheral_impl_standard_pad.hpp>

#include "settings_view_base.hpp"

namespace app::ui {

class StandardPadBindsView : public SettingsViewBase {
public:
    StandardPadBindsView(SharedContext &context);

    void Display(satemu::peripheral::StandardPad &pad);
};

} // namespace app::ui
