#include "system_settings_view.hpp"

#include <app/events/emu_event_factory.hpp>
#include <app/events/gui_event_factory.hpp>

#include <app/ui/widgets/common_widgets.hpp>
#include <app/ui/widgets/datetime_widgets.hpp>
#include <app/ui/widgets/system_widgets.hpp>

#include <util/sdl_file_dialog.hpp>

#include <misc/cpp/imgui_stdlib.h>

#include <fmt/format.h>

using namespace satemu;

namespace app::ui {

SystemSettingsView::SystemSettingsView(SharedContext &context)
    : SettingsViewBase(context) {}

void SystemSettingsView::Display() {
    auto &settings = m_context.settings.system;
    auto &sysConfig = m_context.saturn.configuration.system;
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
            widgets::ExplanationTooltip("Changing this option will cause a hard reset");
        }
        if (ImGui::TableNextColumn()) {
            ImGui::SetNextItemWidth(-(fileSelectorButtonWidth + reloadButtonWidth + itemSpacingWidth * 2));
            std::string biosPath = settings.biosPath.string();
            if (MakeDirty(ImGui::InputText("##bios_path", &biosPath))) {
                settings.biosPath = biosPath;
            }
            ImGui::SameLine();
            if (ImGui::Button("...##bios_path")) {
                m_context.EnqueueEvent(events::gui::OpenFile({
                    .dialogTitle = "Load IPL ROM",
                    .filters = {{"ROM files (*.bin, *.rom)", "bin;rom"}, {"All files (*.*)", "*"}},
                    .userdata = this,
                    .callback = util::WrapSingleSelectionCallback<&SystemSettingsView::ProcessLoadIPLROM,
                                                                  &util::NoopCancelFileDialogCallback,
                                                                  &SystemSettingsView::ProcessLoadIPLROMError>,
                }));
            }
            ImGui::SameLine();
            if (ImGui::Button("Reload")) {
                m_context.EnqueueEvent(events::emu::ReloadIPL());
                m_context.settings.MakeDirty();
            }
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Video standard");
        }
        if (ImGui::TableNextColumn()) {
            MakeDirty(ui::widgets::VideoStandardSelector(m_context));
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Region");
            widgets::ExplanationTooltip("Changing this option will cause a hard reset");
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
    widgets::ExplanationTooltip(
        "Whenever a game disc is loaded, the emulator will automatically switch the system region to match one of the "
        "game's supported regions. The list below allows you to choose the preferred region order. If none of the "
        "preferred regions is supported by the game, the emulator will pick the first region listed on the disc.");

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Preferred region order:");

    // -----------------------------------------------------------------------------------------------------------------

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("Accuracy");
    ImGui::PopFont();

    bool emulateSH2Cache = sysConfig.emulateSH2Cache;
    if (MakeDirty(ImGui::Checkbox("Emulate SH-2 cache", &emulateSH2Cache))) {
        m_context.EnqueueEvent(events::emu::SetEmulateSH2Cache(emulateSH2Cache));
    }
    widgets::ExplanationTooltip("Enables emulation of the SH-2 cache.\n"
                                "A few games require this to work properly.\n"
                                "Reduces emulation performance by about 10%.\n\n"
                                "Upon enabling this option, both SH-2 CPUs' caches will be flushed.");

    // -----------------------------------------------------------------------------------------------------------------

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("Real-Time Clock");
    ImGui::PopFont();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Mode:");
    widgets::ExplanationTooltip("- Host: Syncs the emulated RTC to your system's clock.\n"
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
        widgets::ExplanationTooltip(
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
            widgets::ExplanationTooltip(explanation);
        };

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Hard reset behavior:");
        widgets::ExplanationTooltip("Specifies how the virtual RTC behaves on a hard reset.");

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

void SystemSettingsView::ProcessLoadIPLROM(void *userdata, std::filesystem::path file, int filter) {
    static_cast<SystemSettingsView *>(userdata)->LoadIPLROM(file);
}

void SystemSettingsView::ProcessLoadIPLROMError(void *userdata, const char *message, int filter) {
    static_cast<SystemSettingsView *>(userdata)->ShowIPLROMLoadError(message);
}

void SystemSettingsView::LoadIPLROM(std::filesystem::path file) {
    m_context.EnqueueEvent(events::emu::LoadIPL(file));
}

void SystemSettingsView::ShowIPLROMLoadError(const char *message) {
    m_context.EnqueueEvent(events::gui::ShowError(fmt::format("Could not load IPL ROM: {}", message)));
}

} // namespace app::ui
