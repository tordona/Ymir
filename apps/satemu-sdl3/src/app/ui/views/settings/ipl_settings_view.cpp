#include "ipl_settings_view.hpp"

#include <app/events/gui_event_factory.hpp>

#include <util/sdl_file_dialog.hpp>

#include <misc/cpp/imgui_stdlib.h>

#include <SDL3/SDL_misc.h>

using namespace satemu;

namespace app::ui {

static const char *GetVariantName(db::SystemVariant variant) {
    switch (variant) {
    case db::SystemVariant::None: return "None";
    case db::SystemVariant::Saturn: return "Saturn";
    case db::SystemVariant::HiSaturn: return "HiSaturn";
    case db::SystemVariant::VSaturn: return "V-Saturn";
    case db::SystemVariant::DevKit: return "Dev kit";
    default: return "Unknown";
    }
}

static const char *GetRegionName(db::SystemRegion region) {
    switch (region) {
    case db::SystemRegion::None: return "None";
    case db::SystemRegion::US_EU: return "US/EU";
    case db::SystemRegion::JP: return "Japan";
    case db::SystemRegion::KR: return "South Korea";
    case db::SystemRegion::RegionFree: return "Region-free";
    default: return "Unknown";
    }
}

IPLSettingsView::IPLSettingsView(SharedContext &context)
    : SettingsViewBase(context) {}

void IPLSettingsView::Display() {
    auto &settings = m_context.settings.system.ipl;

    ImGui::TextUnformatted("NOTE: Changing any of these options will cause a hard reset");

    ImGui::Separator();

    const float paddingWidth = ImGui::GetStyle().FramePadding.x;
    const float itemSpacingWidth = ImGui::GetStyle().ItemSpacing.x;
    const float fileSelectorButtonWidth = ImGui::CalcTextSize("...").x + paddingWidth * 2;
    const float reloadButtonWidth = ImGui::CalcTextSize("Reload").x + paddingWidth * 2;
    const float useButtonWidth = ImGui::CalcTextSize("Use").x + paddingWidth * 2;

    std::filesystem::path iplRomsPath = m_context.profile.GetPath(StandardPath::IPLROMImages);

    ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
    ImGui::Text("IPL ROMs in %s", iplRomsPath.string().c_str());
    ImGui::PopTextWrapPos();

    if (ImGui::Button("Open directory")) {
        SDL_OpenURL(fmt::format("file:///{}", iplRomsPath.string()).c_str());
    }
    ImGui::SameLine();
    if (ImGui::Button("Rescan")) {
        m_context.iplRomManager.Scan(iplRomsPath);
    }

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
                    ImGui::Text("%s", GetVariantName(info.info->variant));
                } else {
                    ImGui::TextUnformatted("Unknown");
                }
            }
            if (ImGui::TableNextColumn()) {
                ImGui::AlignTextToFramePadding();
                if (info.info != nullptr) {
                    ImGui::Text("%s", GetRegionName(info.info->region));
                } else {
                    ImGui::TextUnformatted("Unknown");
                }
            }
            if (ImGui::TableNextColumn()) {
                if (ImGui::Button(fmt::format("Use##{}", index).c_str())) {
                    settings.overrideImage = true;
                    settings.path = path;
                    m_context.EnqueueEvent(events::gui::ReloadIPLROM());
                    m_context.settings.MakeDirty();
                }
            }
            ++index;
        }

        ImGui::EndTable();
    }

    ImGui::Separator();

    if (MakeDirty(ImGui::Checkbox("Override IPL ROM", &settings.overrideImage))) {
        m_context.EnqueueEvent(events::gui::ReloadIPLROM());
        m_context.settings.MakeDirty();
    }

    if (!settings.overrideImage) {
        ImGui::BeginDisabled();
    }
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("IPL ROM path");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-(fileSelectorButtonWidth + reloadButtonWidth + itemSpacingWidth * 2));
    std::string iplPath = settings.path.string();
    if (MakeDirty(ImGui::InputText("##ipl_path", &iplPath))) {
        settings.path = iplPath;
    }
    ImGui::SameLine();
    if (ImGui::Button("...##ipl_path")) {
        m_context.EnqueueEvent(events::gui::OpenFile({
            .dialogTitle = "Load IPL ROM",
            .filters = {{"ROM files (*.bin, *.rom)", "bin;rom"}, {"All files (*.*)", "*"}},
            .userdata = this,
            .callback = util::WrapSingleSelectionCallback<&IPLSettingsView::ProcessLoadIPLROM,
                                                          &util::NoopCancelFileDialogCallback,
                                                          &IPLSettingsView::ProcessLoadIPLROMError>,
        }));
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
        m_context.EnqueueEvent(events::gui::ReloadIPLROM());
        m_context.settings.MakeDirty();
    }
    if (!settings.overrideImage) {
        ImGui::EndDisabled();
    }

    ImGui::Separator();

    if (m_context.iplRomPath.empty()) {
        ImGui::TextUnformatted("No IPL ROM loaded");
    } else {
        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
        ImGui::Text("Currently using IPL ROM at %s", m_context.iplRomPath.string().c_str());
        ImGui::PopTextWrapPos();
    }
    const db::IPLROMInfo *info = db::GetIPLROMInfo(m_context.saturn.GetIPLHash());
    if (info != nullptr) {
        ImGui::Text("Version: %s", info->version);
        ImGui::Text("Release date: %04u/%02u/%02u", info->year, info->month, info->day);
        ImGui::Text("Variant: %s", GetVariantName(info->variant));
        ImGui::Text("Region: %s", GetRegionName(info->region));
    } else {
        ImGui::TextUnformatted("Unknown IPL ROM");
    }
}

void IPLSettingsView::ProcessLoadIPLROM(void *userdata, std::filesystem::path file, int filter) {
    static_cast<IPLSettingsView *>(userdata)->LoadIPLROM(file);
}

void IPLSettingsView::ProcessLoadIPLROMError(void *userdata, const char *message, int filter) {
    static_cast<IPLSettingsView *>(userdata)->ShowIPLROMLoadError(message);
}

void IPLSettingsView::LoadIPLROM(std::filesystem::path file) {
    m_context.EnqueueEvent(events::gui::TryLoadIPLROM(file));
}

void IPLSettingsView::ShowIPLROMLoadError(const char *message) {
    m_context.EnqueueEvent(events::gui::ShowError(fmt::format("Could not load IPL ROM: {}", message)));
}

} // namespace app::ui
