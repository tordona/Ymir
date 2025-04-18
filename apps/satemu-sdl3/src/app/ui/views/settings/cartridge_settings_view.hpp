#pragma once

#include "settings_view_base.hpp"

namespace app::ui {

class CartridgeSettingsView : public SettingsViewBase {
public:
    CartridgeSettingsView(SharedContext &context);

    void Display();
};

} // namespace app::ui
