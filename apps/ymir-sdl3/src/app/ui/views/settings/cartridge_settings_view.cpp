#include "cartridge_settings_view.hpp"

#include <ymir/sys/backup_ram.hpp>

#include <app/ui/widgets/cartridge_widgets.hpp>
#include <app/ui/widgets/common_widgets.hpp>

#include <app/events/emu_event_factory.hpp>
#include <app/events/gui_event_factory.hpp>

#include <util/sdl_file_dialog.hpp>

#include <misc/cpp/imgui_stdlib.h>

using namespace ymir;

namespace app::ui {

static const char *GetCartTypeName(Settings::Cartridge::Type type) {
    switch (type) {
    case Settings::Cartridge::Type::None: return "None";
    case Settings::Cartridge::Type::BackupRAM: return "Backup RAM";
    case Settings::Cartridge::Type::DRAM: return "DRAM";
    default: return "Unknown";
    }
}

// ---------------------------------------------------------------------------------------------------------------------

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
            }
        }

        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Insert")) {
        m_context.EnqueueEvent(events::emu::InsertCartridgeFromSettings());
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

    using BUPCap = Settings::Cartridge::BackupRAM::Capacity;

    uint32 currSize = 0;
    std::filesystem::path currPath = "";
    {
        std::unique_lock lock{m_context.locks.cart};
        auto *cart = m_context.saturn.GetCartridge().As<cart::CartType::BackupMemory>();
        if (cart != nullptr) {
            auto &bupMem = cart->GetBackupMemory();
            currSize = bupMem.Size();
            currPath = bupMem.GetPath();
        }
    }

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Capacity:");
    widgets::ExplanationTooltip(
        "This will automatically adjust if you load an existing image from the file selector below.");
    ImGui::SameLine();
    if (ImGui::BeginCombo("##bup_capacity", BupCapacityLongName(settings.capacity), ImGuiComboFlags_WidthFitPreview)) {
        for (auto cap : {BUPCap::_4Mbit, BUPCap::_8Mbit, BUPCap::_16Mbit, BUPCap::_32Mbit}) {
            if (MakeDirty(ImGui::Selectable(BupCapacityLongName(cap), settings.capacity == cap))) {
                settings.capacity = cap;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Image path:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-(fileSelectorButtonWidth + itemSpacingWidth * 2));
    std::string imagePath = settings.imagePath.string();
    if (MakeDirty(ImGui::InputText("##bup_image_path", &imagePath))) {
        settings.imagePath = imagePath;
    }
    ImGui::SameLine();
    if (ImGui::Button("...##bup_image_path")) {
        m_context.EnqueueEvent(events::gui::SaveFile({
            .dialogTitle = "Load backup memory image",
            .defaultPath = settings.imagePath.empty()
                               ? m_context.profile.GetPath(ProfilePath::PersistentState) / "bup-ext.bin"
                               : settings.imagePath,
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

    if (currSize != 0 && CapacityToSize(settings.capacity) != currSize && !currPath.empty() &&
        !settings.imagePath.empty() && currPath == settings.imagePath) {
        ImGui::TextUnformatted("WARNING: Changing the size of the backup memory image will format it!");
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
    }
    ImGui::SameLine();
    if (MakeDirty(ImGui::RadioButton("8 Mbit (1 MiB)", settings.capacity == DRAMCap::_8Mbit))) {
        settings.capacity = DRAMCap::_8Mbit;
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

    // TODO: rework this entire process
    // - everything here should be done by the emulator event
    // - it should also reuse the current backup cartridge and ask the bup::BackupMemory to LoadFrom/CreateFrom
    //   - CreateFrom should temporarily disconnect the file in order to modify its size if it's trying to change the
    //     file it has already loaded

    std::error_code error{};
    bup::BackupMemory bupMem{};
    if (std::filesystem::is_regular_file(file)) {
        // The user selected an existing image. Make sure it's a proper backup image.
        const auto result = bupMem.LoadFrom(file, error);
        switch (result) {
        case bup::BackupMemoryImageLoadResult::Success:
            settings.imagePath = file;
            settings.capacity = SizeToCapacity(bupMem.Size());
            m_context.EnqueueEvent(events::emu::InsertCartridgeFromSettings());
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
    } else {
        // The user wants to create a new image file
        bupMem.CreateFrom(file, CapacityToBupSize(settings.capacity), error);
        if (error) {
            m_context.EnqueueEvent(
                events::gui::ShowError(fmt::format("Could not load backup memory image: {}", error.message())));
        } else {
            settings.imagePath = file;
            m_context.EnqueueEvent(events::emu::InsertCartridgeFromSettings());
            MakeDirty();
        }
    }
}

void CartridgeSettingsView::ShowLoadBackupImageError(const char *message) {
    m_context.EnqueueEvent(events::gui::ShowError(fmt::format("Could not load backup memory image: {}", message)));
}

} // namespace app::ui
