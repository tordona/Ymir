#pragma once

#include "settings_view_base.hpp"

#include <app/ui/widgets/input_widgets.hpp>

namespace app::ui {

class HotkeysSettingsView : public SettingsViewBase {
public:
    HotkeysSettingsView(SharedContext &context);

    void Display();

private:
    widgets::InputCaptureWidget m_inputCaptureWidget;
};

} // namespace app::ui
