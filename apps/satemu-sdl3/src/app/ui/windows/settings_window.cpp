#include "settings_window.hpp"

#include <app/events/emu_event_factory.hpp>
#include <app/events/gui_event_factory.hpp>

#include <app/ui/widgets/datetime_widgets.hpp>
#include <app/ui/widgets/system_widgets.hpp>

#include <misc/cpp/imgui_stdlib.h>

using namespace satemu;

namespace app::ui {

static void ExplanationTooltip(const char *explanation) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(450.0f);
        ImGui::TextUnformatted(explanation);
        ImGui::PopTextWrapPos();
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

    // -----------------------------------------------------------------------------------------------------------------

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
    auto &rtcConfig = m_context.saturn.configuration.rtc;

    // -----------------------------------------------------------------------------------------------------------------

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("General");
    ImGui::PopFont();

    const float paddingWidth = ImGui::GetStyle().FramePadding.x;
    const float itemSpacingWidth = ImGui::GetStyle().ItemSpacing.x;
    const float fileSelectorButtonWidth = ImGui::CalcTextSize("...").x + paddingWidth * 2;
    const float reloadButtonWidth = ImGui::CalcTextSize("Reload").x + paddingWidth * 2;

    if (ImGui::BeginTable("sys_general", 2, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, 0);
        ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("BIOS ROM path");
        }
        if (ImGui::TableNextColumn()) {
            ImGui::SetNextItemWidth(-(fileSelectorButtonWidth + reloadButtonWidth + itemSpacingWidth * 2));
            MakeDirty(ImGui::InputText("##bios_path", &settings.biosPath));
            ImGui::SameLine();
            if (ImGui::Button("...##bios_path")) {
                // TODO: open file selector
                // TODO: settings.biosPath
            }
            ImGui::SameLine();
            if (ImGui::Button("Reload")) {
                // TODO: send event to reload IPL ROM from settings
                // m_context.EnqueueEvent(events::emu::LoadIPL(settings.biosPath));
                m_context.settings.MakeDirty();
            }
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Video standard");
        }
        if (ImGui::TableNextColumn()) {
            if (MakeDirty(ui::widgets::VideoStandardSelector(m_context))) {
                settings.videoStandard = m_context.saturn.GetVideoStandard();
            }
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Region");
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::BeginItemTooltip()) {
                ImGui::TextUnformatted("Changing this option will cause a hard reset");
                ImGui::EndTooltip();
            }
        }
        if (ImGui::TableNextColumn()) {
            // TODO: store in settings
            ui::widgets::RegionSelector(m_context);
        }
        // TODO: auto-detect from game discs + preferred order list

        ImGui::EndTable();
    }

    // -----------------------------------------------------------------------------------------------------------------

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("Behavior");
    ImGui::PopFont();

    MakeDirty(ImGui::Checkbox("Autodetect region from loaded discs",
                              &m_context.saturn.configuration.system.autodetectRegion));
    ExplanationTooltip(
        "Whenever a game disc is loaded, the emulator will automatically switch the system region to match one of the "
        "game's supported regions. The list below allows you to choose the preferred region order. If none of the "
        "preferred regions is supported by the game, the emulator will pick the first region listed on the disc.");

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Preferred region order:");

    // -----------------------------------------------------------------------------------------------------------------

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

    // -----------------------------------------------------------------------------------------------------------------

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("Real-Time Clock");
    ImGui::PopFont();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Mode:");
    ExplanationTooltip("- Host: Syncs the emulated RTC to your system's clock.\n"
                       "- Virtual: Runs a virtual RTC synced to emulation speed.\n\n"
                       "For deterministic behavior, use a virtual RTC synced to a fixed time point on reset.");
    ImGui::SameLine();
    if (MakeDirty(ImGui::RadioButton("Host##rtc", rtcConfig.mode == config::rtc::Mode::Host))) {
        rtcConfig.mode = config::rtc::Mode::Host;
    }
    ImGui::SameLine();
    if (MakeDirty(ImGui::RadioButton("Virtual##rtc", rtcConfig.mode == config::rtc::Mode::Virtual))) {
        rtcConfig.mode = config::rtc::Mode::Virtual;
    }

    auto &rtc = m_context.saturn.SMPC.GetRTC();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Current date/time:");
    ImGui::SameLine();
    auto dateTime = rtc.GetDateTime();
    if (widgets::DateTimeSelector("rtc_curr", dateTime)) {
        rtc.SetDateTime(dateTime);
    }

    if (rtcConfig.mode == config::rtc::Mode::Host) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Host time offset:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        ImGui::DragScalar("##rtc_host_offset", ImGuiDataType_S64, &rtc.HostTimeOffset());
        ImGui::SameLine();
        ImGui::TextUnformatted("seconds");
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            rtc.HostTimeOffset() = 0;
        }
    } else if (rtcConfig.mode == config::rtc::Mode::Virtual) {
        // TODO: request emulator to update date/time so that it is updated in real time
        ExplanationTooltip(
            "This may occasionally stop updating because the virtual RTC is only updated when the game reads from it.");

        if (ImGui::Button("Set to host time##curr_time")) {
            rtc.SetDateTime(util::datetime::host());
        }
        ImGui::SameLine();
        if (ImGui::Button("Set to starting point##curr_time")) {
            rtc.SetDateTime(util::datetime::from_timestamp(rtcConfig.virtHardResetTimestamp));
        }

        using HardResetStrategy = config::rtc::HardResetStrategy;

        auto hardResetOption = [&](const char *name, HardResetStrategy strategy, const char *explanation) {
            if (MakeDirty(ImGui::RadioButton(fmt::format("{}##virt_rtc_reset", name).c_str(),
                                             rtcConfig.virtHardResetStrategy == strategy))) {
                rtcConfig.virtHardResetStrategy = strategy;
            }
            ExplanationTooltip(explanation);
        };

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Hard reset behavior:");
        ExplanationTooltip("Specifies how the virtual RTC behaves on a hard reset.");

        hardResetOption("Preserve current time", HardResetStrategy::Preserve,
                        "The virtual RTC will continue counting from the time point prior to the reset.\n"
                        "The date/time persists between executions of the emulator.");

        hardResetOption("Sync to host time", HardResetStrategy::SyncToHost,
                        "The virtual RTC will reset to the current host RTC time.");

        hardResetOption("Reset to starting point", HardResetStrategy::ResetToFixedTime,
                        "The virtual RTC will reset to the specified starting point.");

        ImGui::Indent();
        {
            auto dateTime = util::datetime::from_timestamp(rtcConfig.virtHardResetTimestamp);
            if (MakeDirty(widgets::DateTimeSelector("virt_base_time", dateTime))) {
                rtcConfig.virtHardResetTimestamp = util::datetime::to_timestamp(dateTime);
            }
            if (MakeDirty(ImGui::Button("Set to host time##virt_base_time"))) {
                rtcConfig.virtHardResetTimestamp = util::datetime::to_timestamp(util::datetime::host());
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
    auto &config = m_context.saturn.configuration.video;

    // -----------------------------------------------------------------------------------------------------------------

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("Display");
    ImGui::PopFont();

    MakeDirty(ImGui::Checkbox("Force integer scaling", &settings.forceIntegerScaling));
    MakeDirty(ImGui::Checkbox("Force aspect ratio", &settings.forceAspectRatio));
    ExplanationTooltip("If disabled, forces square pixels.");
    ImGui::SameLine();
    if (MakeDirty(ImGui::Button("4:3"))) {
        settings.forcedAspect = 4.0 / 3.0;
    }
    ImGui::SameLine();
    if (MakeDirty(ImGui::Button("16:9"))) {
        settings.forcedAspect = 16.0 / 9.0;
    }
    // TODO: aspect ratio selector? slider?

    MakeDirty(ImGui::Checkbox("Auto-fit window to screen", &settings.autoResizeWindow));
    ExplanationTooltip("If forced aspect ratio is disabled, adjusts and recenters the window whenever the display "
                       "resolution changes.");
    ImGui::SameLine();
    if (settings.displayVideoOutputInWindow) {
        ImGui::BeginDisabled();
    }
    if (MakeDirty(ImGui::Button("Fit now"))) {
        m_context.EnqueueEvent(events::gui::FitWindowToScreen());
    }
    if (settings.displayVideoOutputInWindow) {
        ImGui::EndDisabled();
    }

    if (MakeDirty(ImGui::Checkbox("Windowed video output", &settings.displayVideoOutputInWindow))) {
        m_context.EnqueueEvent(events::gui::FitWindowToScreen());
    }

    // -----------------------------------------------------------------------------------------------------------------

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("Performance");
    ImGui::PopFont();

    // TODO: renderer backend options

    bool threadedVDP1 = config.threadedVDP1;
    if (MakeDirty(ImGui::Checkbox("Threaded VDP1 renderer", &threadedVDP1))) {
        m_context.EnqueueEvent(events::emu::EnableThreadedVDP1Rendering(threadedVDP1));
    }
    ExplanationTooltip("Runs the software VDP1 renderer in a dedicated thread.\n"
                       "Slightly improves performance at the cost of accuracy.\n"
                       "A few select games may break when this option is enabled.\n"
                       "When disabled, VDP1 rendering is done on the emulator thread.");

    bool threadedVDP2 = config.threadedVDP2;
    if (MakeDirty(ImGui::Checkbox("Threaded VDP2 renderer", &threadedVDP2))) {
        m_context.EnqueueEvent(events::emu::EnableThreadedVDP2Rendering(threadedVDP2));
    }
    ExplanationTooltip("Runs the software VDP2 renderer in a dedicated thread.\n"
                       "Greatly improves performance at the cost of accuracy.\n"
                       "A few select games may break when this option is enabled.\n"
                       "When disabled, VDP2 rendering is done on the emulator thread.");
}

void SettingsWindow::DrawAudioTab() {
    auto &config = m_context.saturn.configuration.audio;

    // -----------------------------------------------------------------------------------------------------------------

    using InterpMode = config::audio::SampleInterpolationMode;

    auto interpOption = [&](const char *name, InterpMode mode) {
        const std::string label = fmt::format("{}##sample_interp", name);
        ImGui::SameLine();
        if (MakeDirty(ImGui::RadioButton(label.c_str(), config.interpolation == mode))) {
            config.interpolation = mode;
        }
    };

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("Quality");
    ImGui::PopFont();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Interpolation:");
    ExplanationTooltip("- Nearest neighbor: Cheapest option with grittier sounds.\n"
                       "- Linear: Hardware accurate option with softer sounds. (default)");
    interpOption("Nearest neighbor", InterpMode::NearestNeighbor);
    interpOption("Linear", InterpMode::Linear);

    // -----------------------------------------------------------------------------------------------------------------

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("Performance");
    ImGui::PopFont();

    bool threadedSCSP = config.threadedSCSP;
    if (MakeDirty(ImGui::Checkbox("Run the SCSP and sound CPU on a thread", &threadedSCSP))) {
        m_context.EnqueueEvent(events::emu::EnableThreadedSCSP(threadedSCSP));
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
