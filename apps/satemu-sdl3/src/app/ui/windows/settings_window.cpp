#include "settings_window.hpp"

#include <app/events/emu_event_factory.hpp>
#include <app/events/gui_event_factory.hpp>

#include <app/ui/widgets/datetime_widgets.hpp>

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
        m_context.EnqueueEvent(events::emu::UpdateRTCMode());
    }
    ImGui::SameLine();
    if (MakeDirty(ImGui::RadioButton("Virtual##rtc", settings.rtc.mode == RTCMode::Virtual))) {
        settings.rtc.mode = RTCMode::Virtual;
        m_context.EnqueueEvent(events::emu::UpdateRTCMode());
    }

    auto &rtc = m_context.saturn.SMPC.GetRTC();

    // TODO: when RTC is in virtual mode, request emulator to update date/time so that it is updated in real time

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Current date/time:");
    ImGui::SameLine();
    auto dateTime = rtc.GetDateTime();
    if (widgets::DateTimeSelector("rtc_curr", dateTime)) {
        rtc.SetDateTime(dateTime);
        if (settings.rtc.mode == RTCMode::Host) {
            settings.rtc.hostTimeOffset = rtc.GetHostTimeOffset();
            m_context.settings.MakeDirty();
        }
    }

    if (settings.rtc.mode == RTCMode::Host) {
        bool hostTimeOffsetChanged = false;
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Host time offset:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        hostTimeOffsetChanged |=
            ImGui::DragScalar("##rtc_host_offset", ImGuiDataType_S64, &settings.rtc.hostTimeOffset);
        ImGui::SameLine();
        ImGui::TextUnformatted("seconds");
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            settings.rtc.hostTimeOffset = 0;
            hostTimeOffsetChanged = true;
        }
        if (hostTimeOffsetChanged) {
            rtc.SetHostTimeOffset(settings.rtc.hostTimeOffset);
            m_context.settings.MakeDirty();
        }
    } else if (settings.rtc.mode == RTCMode::Virtual) {
        if (ImGui::Button("Set to host time##curr_time")) {
            rtc.SetDateTime(util::datetime::host());
        }
        ImGui::SameLine();
        if (ImGui::Button("Set to starting point##curr_time")) {
            rtc.SetDateTime(util::datetime::from_timestamp(settings.rtc.virtBaseTime));
        }

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Reset behavior:");
        ExplanationTooltip("Specifies the virtual RTC behavior on system reset.");
        if (MakeDirty(
                ImGui::RadioButton("Preserve current time##virt_rtc_reset",
                                   settings.rtc.virtResetBehavior == VirtualRTCResetBehavior::PreserveCurrentTime))) {
            settings.rtc.virtResetBehavior = VirtualRTCResetBehavior::PreserveCurrentTime;
            m_context.EnqueueEvent(events::emu::UpdateRTCResetStrategy());
        }
        ExplanationTooltip("The virtual RTC will continue counting from the time point prior to the reset.");
        if (MakeDirty(ImGui::RadioButton("Sync to host time##virt_rtc_reset",
                                         settings.rtc.virtResetBehavior == VirtualRTCResetBehavior::SyncToHost))) {
            settings.rtc.virtResetBehavior = VirtualRTCResetBehavior::SyncToHost;
            m_context.EnqueueEvent(events::emu::UpdateRTCResetStrategy());
        }
        ExplanationTooltip("The virtual RTC will reset to the current host RTC time.");
        if (MakeDirty(ImGui::RadioButton("Sync to starting point##virt_rtc_reset",
                                         settings.rtc.virtResetBehavior ==
                                             VirtualRTCResetBehavior::SyncToFixedStartingTime))) {
            settings.rtc.virtResetBehavior = VirtualRTCResetBehavior::SyncToFixedStartingTime;
            m_context.EnqueueEvent(events::emu::UpdateRTCResetStrategy());
        }
        ExplanationTooltip("The virtual RTC will reset to the specified starting point.");

        ImGui::Indent();
        {
            auto dateTime = util::datetime::from_timestamp(settings.rtc.virtBaseTime);
            if (MakeDirty(widgets::DateTimeSelector("virt_base_time", dateTime))) {
                settings.rtc.virtBaseTime = util::datetime::to_timestamp(dateTime);
            }
            if (MakeDirty(ImGui::Button("Set to host time##virt_base_time"))) {
                settings.rtc.virtBaseTime = util::datetime::to_timestamp(util::datetime::host());
            }
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
