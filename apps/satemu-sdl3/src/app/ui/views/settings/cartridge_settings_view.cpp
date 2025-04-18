#include "cartridge_settings_view.hpp"

#include <satemu/sys/backup_ram.hpp>

#include <app/ui/widgets/cartridge_widgets.hpp>

#include <app/events/emu_event_factory.hpp>
#include <app/events/gui_event_factory.hpp>

#include <util/sdl_file_dialog.hpp>

#include <misc/cpp/imgui_stdlib.h>

using namespace satemu;

namespace app::ui {

static const char *GetCartTypeName(Settings::Cartridge::Type type) {
    switch (type) {
    case Settings::Cartridge::Type::None: return "None";
    case Settings::Cartridge::Type::BackupRAM: return "Backup RAM";
    case Settings::Cartridge::Type::DRAM: return "DRAM";
    default: return "Unknown";
    }
}

CartridgeSettingsView::CartridgeSettingsView(SharedContext &context)
    : SettingsViewBase(context) {}

void CartridgeSettingsView::Display() {
    auto &settings = m_context.settings.cartridge;

    static constexpr Settings::Cartridge::Type kCartTypes[] = {
        Settings::Cartridge::Type::None,
        Settings::Cartridge::Type::BackupRAM,
        Settings::Cartridge::Type::DRAM,
    };

    ImGui::TextUnformatted("Current cartridge: ");
    ImGui::SameLine(0, 0);
    widgets::CartridgeInfo(m_context);
    ImGui::Separator();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Cartridge type:");
    ImGui::SameLine();
    if (ImGui::BeginCombo("##cart_type", GetCartTypeName(settings.type), ImGuiComboFlags_WidthFitPreview)) {
        for (auto type : kCartTypes) {
            if (MakeDirty(ImGui::Selectable(GetCartTypeName(type), type == settings.type))) {
                settings.type = type;
                m_cartSettingsDirty = true;
            }
        }

        ImGui::EndCombo();
    }
    ImGui::SameLine();
    const bool dirty = m_cartSettingsDirty;
    if (!dirty) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Insert")) {
        m_context.EnqueueEvent(events::emu::InsertCartridgeFromSettings());
        m_cartSettingsDirty = false;
    }
    if (!dirty) {
        ImGui::EndDisabled();
    }

    switch (settings.type) {
    case Settings::Cartridge::Type::None: break;
    case Settings::Cartridge::Type::BackupRAM: DrawBackupRAMSettings(); break;
    case Settings::Cartridge::Type::DRAM: DrawDRAMSettings(); break;
    }
}

void CartridgeSettingsView::DrawBackupRAMSettings() {
    auto &settings = m_context.settings.cartridge.backupRAM;

    const float paddingWidth = ImGui::GetStyle().FramePadding.x;
    const float itemSpacingWidth = ImGui::GetStyle().ItemSpacing.x;
    const float fileSelectorButtonWidth = ImGui::CalcTextSize("...").x + paddingWidth * 2;

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Backup memory image path");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-(fileSelectorButtonWidth + itemSpacingWidth * 2));
    std::string imagePath = settings.imagePath.string();
    if (MakeDirty(ImGui::InputText("##bup_image_path", &imagePath))) {
        settings.imagePath = imagePath;
        m_cartSettingsDirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("...##bup_image_path")) {
        m_context.EnqueueEvent(events::gui::OpenFile({
            .dialogTitle = "Load backup memory image",
            .defaultPath = settings.imagePath,
            .filters = {{"Backup memory image files (*.bin)", "bin"}, {"All files (*.*)", "*"}},
            .userdata = this,
            .callback = util::WrapSingleSelectionCallback<&CartridgeSettingsView::ProcessLoadBackupImage,
                                                          &util::NoopCancelFileDialogCallback,
                                                          &CartridgeSettingsView::ProcessLoadBackupImageError>,
        }));
    }

    if (ImGui::Button("Open backup memory manager")) {
        m_context.EnqueueEvent(events::gui::OpenBackupMemoryManager());
    }
}

void CartridgeSettingsView::DrawDRAMSettings() {
    auto &settings = m_context.settings.cartridge.dram;

    using DRAMCap = Settings::Cartridge::DRAM::Capacity;

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Capacity:");
    ImGui::SameLine();
    if (MakeDirty(ImGui::RadioButton("32 Mbit (4 MiB)", settings.capacity == DRAMCap::_32Mbit))) {
        settings.capacity = DRAMCap::_32Mbit;
        m_cartSettingsDirty = true;
    }
    ImGui::SameLine();
    if (MakeDirty(ImGui::RadioButton("8 Mbit (1 MiB)", settings.capacity == DRAMCap::_8Mbit))) {
        settings.capacity = DRAMCap::_8Mbit;
        m_cartSettingsDirty = true;
    }
}

void CartridgeSettingsView::ProcessLoadBackupImage(void *userdata, std::filesystem::path file, int filter) {
    static_cast<CartridgeSettingsView *>(userdata)->LoadBackupImage(file);
}

void CartridgeSettingsView::ProcessLoadBackupImageError(void *userdata, const char *message, int filter) {
    static_cast<CartridgeSettingsView *>(userdata)->ShowLoadBackupImageError(message);
}

void CartridgeSettingsView::LoadBackupImage(std::filesystem::path file) {
    auto &settings = m_context.settings.cartridge.backupRAM;

    std::error_code error{};
    bup::BackupMemory bupMem{};
    const auto result = bupMem.LoadFrom(file, error);
    switch (result) {
    case bup::BackupMemoryImageLoadResult::Success:
        settings.imagePath = file;
        m_cartSettingsDirty = true;
        MakeDirty();
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
}

void CartridgeSettingsView::ShowLoadBackupImageError(const char *message) {
    m_context.EnqueueEvent(events::gui::ShowError(fmt::format("Could not load backup memory image: {}", message)));
}

} // namespace app::ui
