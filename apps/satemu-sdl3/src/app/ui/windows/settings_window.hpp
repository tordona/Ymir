#pragma once

#include <app/ui/window_base.hpp>

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
    void DrawGeneralTab();
    void DrawSystemTab();
    void DrawInputTab();
    void DrawVideoTab();
    void DrawAudioTab();

    bool MakeDirty(bool changed);

    SettingsTab m_selectedTab = SettingsTab::None;
};

} // namespace app::ui
