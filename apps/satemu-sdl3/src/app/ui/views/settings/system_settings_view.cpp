#include "system_settings_view.hpp"

#include <app/events/emu_event_factory.hpp>
#include <app/events/gui_event_factory.hpp>

#include <app/ui/widgets/common_widgets.hpp>
#include <app/ui/widgets/datetime_widgets.hpp>
#include <app/ui/widgets/system_widgets.hpp>

#include <util/regions.hpp>
#include <util/sdl_file_dialog.hpp>

#include <misc/cpp/imgui_stdlib.h>

#include <fmt/format.h>

#include <set>

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
    ImGui::SeparatorText("IPL ROM");
    ImGui::PopFont();

    ImGui::TextUnformatted("NOTE: Changing any of these options will cause a hard reset");

    if (m_context.iplRomPath.empty()) {
        ImGui::TextUnformatted("No IPL ROM loaded");
    } else {
        ImGui::Text("Currently using IPL ROM at %s", m_context.iplRomPath.string().c_str());
    }

    ImGui::Separator();

    const float paddingWidth = ImGui::GetStyle().FramePadding.x;
    const float itemSpacingWidth = ImGui::GetStyle().ItemSpacing.x;
    const float fileSelectorButtonWidth = ImGui::CalcTextSize("...").x + paddingWidth * 2;
    const float reloadButtonWidth = ImGui::CalcTextSize("Reload").x + paddingWidth * 2;
    const float useButtonWidth = ImGui::CalcTextSize("Use").x + paddingWidth * 2;

    std::filesystem::path iplRomsPath = m_context.profile.GetPath(StandardPath::IPLROMImages);
    if (ImGui::Button("Rescan##ipl_roms")) {
        m_context.iplRomManager.Scan(iplRomsPath);
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("ROMs in profile directory:");
    ImGui::Text("%s", iplRomsPath.string().c_str());
    int index = 0;
    if (ImGui::BeginTable("sys_ipl_roms", 6, ImGuiTableFlags_ScrollY, ImVec2(0, 250))) {
        ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Version", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, 75);
        ImGui::TableSetupColumn("Variant", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Region", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("##use", ImGuiTableColumnFlags_WidthFixed, useButtonWidth);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        for (auto &[path, info] : m_context.iplRomManager.GetROMs()) {
            ImGui::TableNextRow();

            if (ImGui::TableNextColumn()) {
                std::filesystem::path relativePath = std::filesystem::relative(path, iplRomsPath);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("%s", relativePath.string().c_str());
            }
            if (ImGui::TableNextColumn()) {
                ImGui::AlignTextToFramePadding();
                if (info.info != nullptr) {
                    ImGui::Text("%s", info.info->version);
                } else {
                    ImGui::TextUnformatted("-");
                }
            }
            if (ImGui::TableNextColumn()) {
                ImGui::AlignTextToFramePadding();
                if (info.info != nullptr) {
                    ImGui::Text("%04u/%02u/%02u", info.info->year, info.info->month, info.info->day);
                } else {
                    ImGui::TextUnformatted("-");
                }
            }
            if (ImGui::TableNextColumn()) {
                ImGui::AlignTextToFramePadding();
                if (info.info != nullptr) {
                    switch (info.info->variant) {
                    case db::SystemVariant::None: ImGui::TextUnformatted("None"); break;
                    case db::SystemVariant::Saturn: ImGui::TextUnformatted("Saturn"); break;
                    case db::SystemVariant::HiSaturn: ImGui::TextUnformatted("HiSaturn"); break;
                    case db::SystemVariant::VSaturn: ImGui::TextUnformatted("V-Saturn"); break;
                    case db::SystemVariant::DevKit: ImGui::TextUnformatted("Dev kit"); break;
                    default: ImGui::TextUnformatted("Unknown"); break;
                    }
                } else {
                    ImGui::TextUnformatted("Unknown");
                }
            }
            if (ImGui::TableNextColumn()) {
                ImGui::AlignTextToFramePadding();
                if (info.info != nullptr) {
                    switch (info.info->region) {
                    case db::SystemRegion::None: ImGui::TextUnformatted("None"); break;
                    case db::SystemRegion::US_EU: ImGui::TextUnformatted("US/EU"); break;
                    case db::SystemRegion::JP: ImGui::TextUnformatted("Japan"); break;
                    case db::SystemRegion::KR: ImGui::TextUnformatted("South Korea"); break;
                    case db::SystemRegion::RegionFree: ImGui::TextUnformatted("Region-free"); break;
                    default: ImGui::TextUnformatted("Unknown"); break;
                    }
                } else {
                    ImGui::TextUnformatted("Unknown");
                }
            }
            if (ImGui::TableNextColumn()) {
                if (ImGui::Button(fmt::format("Use##{}", index).c_str())) {
                    settings.ipl.overrideImage = true;
                    settings.ipl.path = path;
                    m_context.EnqueueEvent(events::gui::ReloadIPLROM());
                    m_context.settings.MakeDirty();
                }
            }
            ++index;
        }

        ImGui::EndTable();
    }

    ImGui::Separator();

    if (MakeDirty(ImGui::Checkbox("Override IPL ROM", &settings.ipl.overrideImage))) {
        m_context.EnqueueEvent(events::gui::ReloadIPLROM());
        m_context.settings.MakeDirty();
    }

    if (!settings.ipl.overrideImage) {
        ImGui::BeginDisabled();
    }
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("IPL ROM path");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-(fileSelectorButtonWidth + reloadButtonWidth + itemSpacingWidth * 2));
    std::string iplPath = settings.ipl.path.string();
    if (MakeDirty(ImGui::InputText("##ipl_path", &iplPath))) {
        settings.ipl.path = iplPath;
    }
    ImGui::SameLine();
    if (ImGui::Button("...##ipl_path")) {
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
        m_context.EnqueueEvent(events::gui::ReloadIPLROM());
        m_context.settings.MakeDirty();
    }
    if (!settings.ipl.overrideImage) {
        ImGui::EndDisabled();
    }

    // -----------------------------------------------------------------------------------------------------------------

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("Region");
    ImGui::PopFont();

    if (ImGui::BeginTable("sys_region", 2, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, 0);
        ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);

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
            ui::widgets::RegionSelector(m_context);
        }

        ImGui::EndTable();
    }

    MakeDirty(ImGui::Checkbox("Autodetect region from loaded discs",
                              &m_context.saturn.configuration.system.autodetectRegion));
    widgets::ExplanationTooltip(
        "Whenever a game disc is loaded, the emulator will automatically switch the system region to match one of the "
        "game's supported regions. The list below allows you to choose the preferred region order. If none of the "
        "preferred regions is supported by the game, the emulator will pick the first region listed on the disc.");

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Preferred region order:");

    std::vector<config::sys::Region> prefRgnOrder{};
    {
        // Set of all valid regions
        std::set<config::sys::Region> validRegions{config::sys::Region::Japan, config::sys::Region::NorthAmerica,
                                                   config::sys::Region::AsiaNTSC, config::sys::Region::EuropePAL};

        // Build list of regions from setting using only valid options
        for (auto &region : sysConfig.preferredRegionOrder.Get()) {
            if (validRegions.erase(region)) {
                prefRgnOrder.push_back(region);
            }
        }

        // Add any missing regions to the end of the list
        prefRgnOrder.insert(prefRgnOrder.end(), validRegions.begin(), validRegions.end());
    }

    if (ImGui::BeginListBox("##pref_rgn_order", ImVec2(150, ImGui::GetFrameHeight() * 4))) {
        ImGui::PushItemFlag(ImGuiItemFlags_AllowDuplicateId, true);
        bool changed = false;
        for (int n = 0; n < prefRgnOrder.size(); n++) {
            config::sys::Region item = prefRgnOrder[n];
            ImGui::Selectable(util::RegionToString(item).c_str());

            if (ImGui::IsItemActive() && !ImGui::IsItemHovered()) {
                int n_next = n + (ImGui::GetMouseDragDelta(0).y < 0.f ? -1 : 1);
                if (n_next >= 0 && n_next < prefRgnOrder.size()) {
                    prefRgnOrder[n] = prefRgnOrder[n_next];
                    prefRgnOrder[n_next] = item;
                    ImGui::ResetMouseDragDelta();
                    changed = true;
                }
            }
        }
        ImGui::PopItemFlag();

        if (changed) {
            sysConfig.preferredRegionOrder = prefRgnOrder;
            MakeDirty();
        }

        ImGui::EndListBox();
    }

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
    m_context.EnqueueEvent(events::gui::TryLoadIPLROM(file));
}

void SystemSettingsView::ShowIPLROMLoadError(const char *message) {
    m_context.EnqueueEvent(events::gui::ShowError(fmt::format("Could not load IPL ROM: {}", message)));
}

} // namespace app::ui
