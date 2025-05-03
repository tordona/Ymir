#pragma once

#include "gui_event.hpp"

#include <app/input/input_action.hpp>

namespace app::events::gui {

inline GUIEvent LoadDisc() {
    return {.type = GUIEvent::Type::LoadDisc};
}

inline GUIEvent OpenBackupMemoryCartFileDialog() {
    return {.type = GUIEvent::Type::OpenBackupMemoryCartFileDialog};
}

inline GUIEvent OpenBackupMemoryManager() {
    return {.type = GUIEvent::Type::OpenBackupMemoryManager};
}

inline GUIEvent OpenPeripheralBindsEditor(uint32 portIndex, uint32 slotIndex) {
    return {.type = GUIEvent::Type::OpenPeripheralBindsEditor,
            .value = PeripheralBindsParams{.portIndex = portIndex, .slotIndex = slotIndex}};
}

inline GUIEvent OpenSettings(ui::SettingsTab tab) {
    return {.type = GUIEvent::Type::OpenSettings, .value = tab};
}

inline GUIEvent OpenFile(FileDialogParams &&params) {
    return {.type = GUIEvent::Type::OpenFile, .value = std::move(params)};
}

inline GUIEvent OpenManyFiles(FileDialogParams &&params) {
    return {.type = GUIEvent::Type::OpenManyFiles, .value = std::move(params)};
}

inline GUIEvent SaveFile(FileDialogParams &&params) {
    return {.type = GUIEvent::Type::SaveFile, .value = std::move(params)};
}

inline GUIEvent SelectFolder(FolderDialogParams &&params) {
    return {.type = GUIEvent::Type::SelectFolder, .value = std::move(params)};
}

inline GUIEvent SetProcessPriority(bool boost) {
    return {.type = GUIEvent::Type::SetProcessPriority, .value = boost};
}

inline GUIEvent FitWindowToScreen() {
    return {.type = GUIEvent::Type::FitWindowToScreen};
}

inline GUIEvent RebindInputs() {
    return {.type = GUIEvent::Type::RebindInputs};
}

inline GUIEvent RebindAction(input::Action action) {
    return {.type = GUIEvent::Type::RebindAction, .value = action};
}

inline GUIEvent ShowError(std::string message) {
    return {.type = GUIEvent::Type::ShowErrorMessage, .value = message};
}

inline GUIEvent StateSaved(uint32 slot) {
    return {.type = GUIEvent::Type::StateSaved, .value = slot};
}

inline GUIEvent EnableRewindBuffer(bool enable) {
    return {.type = GUIEvent::Type::EnableRewindBuffer, .value = enable};
}

inline GUIEvent TryLoadIPLROM(std::filesystem::path path) {
    return {.type = GUIEvent::Type::TryLoadIPLROM, .value = path};
}

inline GUIEvent ReloadIPLROM() {
    return {.type = GUIEvent::Type::ReloadIPLROM};
}

} // namespace app::events::gui
