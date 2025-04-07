#pragma once

#include <app/ui/window_base.hpp>

#include <app/ui/views/settings/audio_settings_view.hpp>
#include <app/ui/views/settings/general_settings_view.hpp>
#include <app/ui/views/settings/input_settings_view.hpp>
#include <app/ui/views/settings/system_settings_view.hpp>
#include <app/ui/views/settings/video_settings_view.hpp>

namespace app::ui {

enum class SettingsTab { None, General, System, Input, Video, Audio };

class SettingsWindow : public WindowBase {
public:
    SettingsWindow(SharedContext &context);

    void OpenTab(SettingsTab tab);

protected:
    void PrepareWindow() override;
    void DrawContents() override;

private:
    SettingsTab m_selectedTab = SettingsTab::None;

    GeneralSettingsView m_generalSettingsView;
    SystemSettingsView m_systemSettingsView;
    InputSettingsView m_inputSettingsView;
    VideoSettingsView m_videoSettingsView;
    AudioSettingsView m_audioSettingsView;
};

} // namespace app::ui
