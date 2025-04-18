#include "system_settings_view.hpp"

#include <app/events/emu_event_factory.hpp>
#include <app/events/gui_event_factory.hpp>

#include <app/ui/widgets/common_widgets.hpp>
#include <app/ui/widgets/datetime_widgets.hpp>
#include <app/ui/widgets/system_widgets.hpp>

#include <util/regions.hpp>
#include <util/sdl_file_dialog.hpp>

#include <misc/cpp/imgui_stdlib.h>

#include <satemu/util/size_ops.hpp>

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

    const float paddingWidth = ImGui::GetStyle().FramePadding.x;
    const float itemSpacingWidth = ImGui::GetStyle().ItemSpacing.x;
    const float fileSelectorButtonWidth = ImGui::CalcTextSize("...").x + paddingWidth * 2;

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

    // -----------------------------------------------------------------------------------------------------------------

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("Internal backup memory");
    ImGui::PopFont();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Image path");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-(fileSelectorButtonWidth + itemSpacingWidth * 2));
    std::string imagePath = settings.internalBackupRAMImagePath.string();
    if (MakeDirty(ImGui::InputText("##bup_image_path", &imagePath))) {
        settings.internalBackupRAMImagePath = imagePath;
        m_bupSettingsDirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("...##bup_image_path")) {
        m_context.EnqueueEvent(events::gui::OpenFile({
            .dialogTitle = "Load backup memory image",
            .defaultPath = settings.internalBackupRAMImagePath.empty()
                               ? m_context.profile.GetPath(ProfilePath::PersistentState) / "bup-int.bin"
                               : settings.internalBackupRAMImagePath,
            .filters = {{"Backup memory image files (*.bin)", "bin"}, {"All files (*.*)", "*"}},
            .userdata = this,
            .callback = util::WrapSingleSelectionCallback<&SystemSettingsView::ProcessLoadBackupImage,
                                                          &util::NoopCancelFileDialogCallback,
                                                          &SystemSettingsView::ProcessLoadBackupImageError>,
        }));
    }

    const bool dirty = m_bupSettingsDirty;
    if (!dirty) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Load")) {
        m_context.EnqueueEvent(events::emu::LoadInternalBackupMemory());
        m_bupSettingsDirty = false;
    }
    if (!dirty) {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    if (ImGui::Button("Open backup memory manager")) {
        m_context.EnqueueEvent(events::gui::OpenBackupMemoryManager());
    }
}

void SystemSettingsView::ProcessLoadBackupImage(void *userdata, std::filesystem::path file, int filter) {
    static_cast<SystemSettingsView *>(userdata)->LoadBackupImage(file);
}

void SystemSettingsView::ProcessLoadBackupImageError(void *userdata, const char *message, int filter) {
    static_cast<SystemSettingsView *>(userdata)->ShowLoadBackupImageError(message);
}

void SystemSettingsView::LoadBackupImage(std::filesystem::path file) {
    auto &settings = m_context.settings.system;

    if (std::filesystem::is_regular_file(file)) {
        // The user selected an existing image. Make sure it's a proper internal backup image.
        std::error_code error{};
        bup::BackupMemory bupMem{};
        const auto result = bupMem.LoadFrom(file, error);
        switch (result) {
        case bup::BackupMemoryImageLoadResult::Success:
            if (bupMem.Size() == 32_KiB) {
                settings.internalBackupRAMImagePath = file;
                m_bupSettingsDirty = false;
                m_context.EnqueueEvent(events::emu::LoadInternalBackupMemory());
                MakeDirty();
            } else {
                m_context.EnqueueEvent(
                    events::gui::ShowError(fmt::format("Could not load backup memory image: Invalid image size")));
            }
            break;
        case bup::BackupMemoryImageLoadResult::FilesystemError:
            if (error) {
                m_context.EnqueueEvent(
                    events::gui::ShowError(fmt::format("Could not load backup memory image: {}", error.message())));
            } else {
                m_context.EnqueueEvent(events::gui::ShowError(
                    fmt::format("Could not load backup memory image: Unspecified file system error")));
            }
            break;
        case bup::BackupMemoryImageLoadResult::InvalidSize:
            m_context.EnqueueEvent(
                events::gui::ShowError(fmt::format("Could not load backup memory image: Invalid image size")));
            break;
        default:
            m_context.EnqueueEvent(
                events::gui::ShowError(fmt::format("Could not load backup memory image: Unexpected error")));
            break;
        }
    } else {
        // The user wants to create a new image file
        settings.internalBackupRAMImagePath = file;
        m_bupSettingsDirty = false;
        m_context.EnqueueEvent(events::emu::LoadInternalBackupMemory());
        MakeDirty();
    }
}

void SystemSettingsView::ShowLoadBackupImageError(const char *message) {
    m_context.EnqueueEvent(events::gui::ShowError(fmt::format("Could not load backup memory image: {}", message)));
}

} // namespace app::ui
