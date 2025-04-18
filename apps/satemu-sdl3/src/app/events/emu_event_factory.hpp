#pragma once

#include <satemu/sys/backup_ram.hpp>
#include <satemu/sys/clocks.hpp>

#include <satemu/hw/smpc/peripheral/peripheral_defs.hpp>

#include "emu_event.hpp"

#include <filesystem>

namespace app::events::emu {

inline EmuEvent FactoryReset() {
    return {.type = EmuEvent::Type::FactoryReset};
}

inline EmuEvent HardReset() {
    return {.type = EmuEvent::Type::HardReset};
}

inline EmuEvent SoftReset() {
    return {.type = EmuEvent::Type::SoftReset};
}

inline EmuEvent SetResetButton(bool resetLevel) {
    return {.type = EmuEvent::Type::SetResetButton, .value = resetLevel};
}

inline EmuEvent SetPaused(bool paused) {
    return {.type = EmuEvent::Type::SetPaused, .value = paused};
}

inline EmuEvent ForwardFrameStep() {
    return {.type = EmuEvent::Type::ForwardFrameStep};
}

inline EmuEvent ReverseFrameStep() {
    return {.type = EmuEvent::Type::ReverseFrameStep};
}

inline EmuEvent OpenCloseTray() {
    return {.type = EmuEvent::Type::OpenCloseTray};
}

inline EmuEvent LoadDisc(std::string path) {
    return {.type = EmuEvent::Type::LoadDisc, .value = path};
}

inline EmuEvent EjectDisc() {
    return {.type = EmuEvent::Type::EjectDisc};
}

inline EmuEvent RemoveCartridge() {
    return {.type = EmuEvent::Type::RemoveCartridge};
}

inline EmuEvent ReplaceInternalBackupMemory(satemu::bup::BackupMemory &&bupMem) {
    return {.type = EmuEvent::Type::ReplaceInternalBackupMemory, .value = std::move(bupMem)};
}

inline EmuEvent ReplaceExternalBackupMemory(satemu::bup::BackupMemory &&bupMem) {
    return {.type = EmuEvent::Type::ReplaceExternalBackupMemory, .value = std::move(bupMem)};
}

inline EmuEvent RunFunction(std::function<void(SharedContext &)> &&fn) {
    return {.type = EmuEvent::Type::RunFunction, .value = std::move(fn)};
}

inline EmuEvent SetThreadPriority(bool boost) {
    return {.type = EmuEvent::Type::SetThreadPriority, .value = boost};
}

inline EmuEvent Shutdown() {
    return {.type = EmuEvent::Type::Shutdown};
}

// -----------------------------------------------------------------------------
// Specialized event factories

EmuEvent SetClockSpeed(satemu::sys::ClockSpeed clockSpeed);
EmuEvent SetVideoStandard(satemu::config::sys::VideoStandard videoStandard);
EmuEvent SetAreaCode(uint8 areaCode);

EmuEvent SetDebugTrace(bool enable);
EmuEvent DumpMemory();

EmuEvent InsertPort1Peripheral(satemu::peripheral::PeripheralType type);
EmuEvent InsertPort2Peripheral(satemu::peripheral::PeripheralType type);

EmuEvent InsertBackupMemoryCartridge(std::filesystem::path path);
EmuEvent Insert8MbitDRAMCartridge();
EmuEvent Insert32MbitDRAMCartridge();
EmuEvent InsertCartridgeFromSettings();

EmuEvent DeleteBackupFile(std::string filename, bool external);
EmuEvent FormatBackupMemory(bool external);

EmuEvent SetEmulateSH2Cache(bool enable);

EmuEvent EnableThreadedVDPRendering(bool enable);

EmuEvent EnableThreadedSCSP(bool enable);

EmuEvent LoadState(uint32 slot);
EmuEvent SaveState(uint32 slot);

} // namespace app::events::emu
