#pragma once

#include "settings_view_base.hpp"

namespace app::ui {

class SystemSettingsView : public SettingsViewBase {
public:
    SystemSettingsView(SharedContext &context);

    void Display();
};

} // namespace app::ui
