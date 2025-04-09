#pragma once

#include "settings_view_base.hpp"

namespace app::ui {

class HotkeysSettingsView : public SettingsViewBase {
public:
    HotkeysSettingsView(SharedContext &context);

    void Display();
};

} // namespace app::ui
