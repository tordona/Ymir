#include "settings_window.hpp"

#include <app/events/emu_event_factory.hpp>
#include <app/events/gui_event_factory.hpp>

namespace app::ui {

static void ExplanationTooltip(const char *explanation) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip()) {
        ImGui::TextUnformatted(explanation);
        ImGui::EndTooltip();
    }
}

SettingsWindow::SettingsWindow(SharedContext &context)
    : WindowBase(context) {

    m_windowConfig.name = "Settings";
}

void SettingsWindow::OpenTab(SettingsTab tab) {
    Open = true;
    m_selectedTab = tab;
    RequestFocus();
}

void SettingsWindow::PrepareWindow() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(500, 300), ImVec2(FLT_MAX, FLT_MAX));
    /*auto *vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f),
                            ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));*/
}

void SettingsWindow::DrawContents() {
    auto tabFlag = [&](SettingsTab tab) {
        if (m_selectedTab == tab) {
            return ImGuiTabItemFlags_SetSelected;
        } else {
            return ImGuiTabItemFlags_None;
        }
    };

    if (ImGui::BeginTabBar("settings_tabs")) {
        if (ImGui::BeginTabItem("General", nullptr, tabFlag(SettingsTab::General))) {
            DrawGeneralTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("System", nullptr, tabFlag(SettingsTab::System))) {
            DrawSystemTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Input", nullptr, tabFlag(SettingsTab::Input))) {
            DrawInputTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Video", nullptr, tabFlag(SettingsTab::Video))) {
            DrawVideoTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Audio", nullptr, tabFlag(SettingsTab::Audio))) {
            DrawAudioTab();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    m_selectedTab = SettingsTab::None;
}

void SettingsWindow::DrawGeneralTab() {
    auto &settings = m_context.settings.general;

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("Performance");
    ImGui::PopFont();

    if (MakeDirty(ImGui::Checkbox("Boost process priority", &settings.boostProcessPriority))) {
        m_context.EnqueueEvent(events::gui::SetProcessPriority(settings.boostProcessPriority));
    }
    ExplanationTooltip("Increases the process's priority level, which may help reduce stuttering.");

    if (MakeDirty(ImGui::Checkbox("Boost emulator thread priority", &settings.boostEmuThreadPriority))) {
        m_context.EnqueueEvent(events::emu::SetThreadPriority(settings.boostEmuThreadPriority));
    }
    ExplanationTooltip("Increases the emulator thread's priority, which may help reduce jitter.");

    MakeDirty(ImGui::Checkbox("Preload disc images to RAM", &settings.preloadDiscImagesToRAM));
    ExplanationTooltip("Preloads the entire disc image to memory.\n"
                       "May help reduce stuttering if you're loading images from a slow disk or from the network.");
}

void SettingsWindow::DrawSystemTab() {
    auto &settings = m_context.settings.system;

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("General");
    ImGui::PopFont();

    // TODO: settings.biosPath

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("Accuracy");
    ImGui::PopFont();

    if (MakeDirty(ImGui::Checkbox("Emulate SH-2 cache", &settings.emulateSH2Cache))) {
        m_context.EnqueueEvent(events::emu::SetEmulateSH2Cache(settings.emulateSH2Cache));
    }
    ExplanationTooltip("Enables emulation of the SH-2 cache.\n"
                       "A few games require this to work properly.\n"
                       "Reduces emulation performance by about 10%.\n\n"
                       "Upon enabling this option, both SH-2 CPUs' caches will be flushed.");

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("Real-Time Clock");
    ImGui::PopFont();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Mode:");
    ExplanationTooltip("- Host: Syncs the emulated RTC to your system's clock.\n"
                       "- Virtual: Runs a virtual RTC synced to emulation speed.\n\n"
                       "For deterministic behavior, use a virtual RTC synced to a fixed time point on reset.");
    ImGui::SameLine();
    if (MakeDirty(ImGui::RadioButton("Host##rtc", settings.rtc.mode == RTCMode::Host))) {
        settings.rtc.mode = RTCMode::Host;
        // TODO: configure RTC
    }
    ImGui::SameLine();
    if (MakeDirty(ImGui::RadioButton("Virtual##rtc", settings.rtc.mode == RTCMode::Virtual))) {
        settings.rtc.mode = RTCMode::Virtual;
        // TODO: configure RTC
    }

    if (settings.rtc.mode == RTCMode::Host) {
        // TODO: settings.rtc.hostTimeOffset;
    } else if (settings.rtc.mode == RTCMode::Virtual) {
        if (MakeDirty(ImGui::SliderFloat("Time scale", &settings.rtc.emuTimeScale, -10.0f, 10.0f, "%.2fx",
                                         ImGuiSliderFlags_AlwaysClamp))) {
            // TODO: send event to configure time scale
        }
        ExplanationTooltip("Affects the virtual RTC's speed relative to the master clock.\n\n"
                           "Note that the virtual RTC will still follow emulation speed:\n"
                           "- If the emulator is paused, the virtual RTC clock is also paused\n"
                           "- If the emulator is fast-forwarding, the virtual RTC clock will also run faster");

        ImGui::TextUnformatted("Reset behavior:");
        ExplanationTooltip("Specifies the virtual RTC behavior on system reset.");
        if (MakeDirty(
                ImGui::RadioButton("Preserve current time##emu_rtc_reset",
                                   settings.rtc.emuResetBehavior == VirtualRTCResetBehavior::PreserveCurrentTime))) {
            settings.rtc.emuResetBehavior = VirtualRTCResetBehavior::PreserveCurrentTime;
            // TODO: configure RTC
        }
        ExplanationTooltip("The virtual RTC will continue counting from the time point prior to the reset.");
        if (MakeDirty(ImGui::RadioButton("Sync to host time##emu_rtc_reset",
                                         settings.rtc.emuResetBehavior == VirtualRTCResetBehavior::SyncToHost))) {
            settings.rtc.emuResetBehavior = VirtualRTCResetBehavior::SyncToHost;
            // TODO: configure RTC
        }
        ExplanationTooltip("The virtual RTC will reset to the current host RTC time.");
        if (MakeDirty(ImGui::RadioButton("Sync to starting point##emu_rtc_reset",
                                         settings.rtc.emuResetBehavior ==
                                             VirtualRTCResetBehavior::SyncToFixedStartingTime))) {
            settings.rtc.emuResetBehavior = VirtualRTCResetBehavior::SyncToFixedStartingTime;
            // TODO: configure RTC
        }
        ExplanationTooltip("The virtual RTC will reset to the specified starting point.");

        ImGui::Indent();
        {
            // TODO: settings.rtc.emuBaseTime;
        }
        ImGui::Unindent();
    }
}

void SettingsWindow::DrawInputTab() {
    /*auto &settings = m_context.settings.input;

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("Bindings");
    ImGui::PopFont();*/
}

void SettingsWindow::DrawVideoTab() {
    auto &settings = m_context.settings.video;

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("Performance");
    ImGui::PopFont();

    if (MakeDirty(ImGui::Checkbox("Run the software renderer on a thread", &settings.threadedRendering))) {
        // TODO: send event
        // m_context.EnqueueEvent(events::emu::SetThreadedRendering(settings.threadedRendering));
    }
    ExplanationTooltip("Greatly improves performance at the cost of accuracy.\n"
                       "A few select games may break when this option is enabled.");

    if (!settings.threadedRendering) {
        ImGui::BeginDisabled();
    }
    ImGui::Indent();
    {
        if (MakeDirty(ImGui::Checkbox("Render VDP1 on the renderer thread", &settings.threadedVDP1))) {
            // TODO: send event
            // m_context.EnqueueEvent(events::emu::SetThreadedVDP1Rendering(settings.threadedVDP1));
        }
        ExplanationTooltip("Moves VDP1 rendering to the renderer thread.\n"
                           "Slightly improves performance at the cost of accuracy.\n"
                           "A few select games may break when this option is enabled.\n"
                           "When disabled, VDP1 rendering is done on the emulator thread.");
    }
    ImGui::Unindent();
    if (!settings.threadedRendering) {
        ImGui::EndDisabled();
    }
}

void SettingsWindow::DrawAudioTab() {
    auto &settings = m_context.settings.audio;

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("Quality");
    ImGui::PopFont();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Interpolation:");
    ExplanationTooltip("- Nearest neighbor: Cheapest option that sounds harsh and retro.\n"
                       "- Linear: Softens sounds. Hardware accurate.");
    ImGui::SameLine();
    if (MakeDirty(ImGui::RadioButton("Nearest neighbor##sound_interp",
                                     settings.interpolationMode == AudioInterpolationMode::Nearest))) {
        settings.interpolationMode = AudioInterpolationMode::Nearest;
        // TODO: configure RTC
    }
    ImGui::SameLine();
    if (MakeDirty(
            ImGui::RadioButton("Linear##sound_interp", settings.interpolationMode == AudioInterpolationMode::Linear))) {
        settings.interpolationMode = AudioInterpolationMode::Linear;
        // TODO: configure RTC
    }

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("Performance");
    ImGui::PopFont();

    if (MakeDirty(ImGui::Checkbox("Run the SCSP and sound CPU on a thread", &settings.threadedSCSP))) {
        // TODO: send event
        // m_context.EnqueueEvent(events::emu::SetThreadedSCSP(settings.threadedSCSP));
    }
    ExplanationTooltip("Improves performance at the cost of accuracy.\n"
                       "A few select games may break when this option is enabled.");
}

bool SettingsWindow::MakeDirty(bool changed) {
    if (changed) {
        m_context.settings.MakeDirty();
    }
    return changed;
}

} // namespace app::ui
