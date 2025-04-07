#pragma once

#include "settings_view_base.hpp"

namespace app::ui {

class GeneralSettingsView : public SettingsViewBase {
public:
    GeneralSettingsView(SharedContext &context);

    void Display();
};

} // namespace app::ui
