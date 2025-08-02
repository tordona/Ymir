#include "shared_context.hpp"

#include <ymir/sys/saturn.hpp>

namespace app {

SharedContext::SharedContext() {
    saturn.instance = std::make_unique<ymir::Saturn>();

    settings.BindConfiguration(saturn.instance->configuration);

    static constexpr auto kRtMidiAPI = RtMidi::Api::UNSPECIFIED;

    midi.midiInput = std::make_unique<RtMidiIn>(kRtMidiAPI, "Ymir MIDI input client");
    midi.midiOutput = std::make_unique<RtMidiOut>(kRtMidiAPI, "Ymir MIDI output client");
    midi.midiInput->ignoreTypes(false, false, false);
}

SharedContext::~SharedContext() = default;

std::filesystem::path SharedContext::GetGameFileName() const {
    // Use serial number + disc title if available
    {
        std::unique_lock lock{locks.disc};
        const auto &disc = saturn.instance->CDBlock.GetDisc();
        if (!disc.sessions.empty() && !disc.header.productNumber.empty()) {
            if (!disc.header.gameTitle.empty()) {
                std::string title = disc.header.gameTitle;
                // Clean up invalid characters
                std::transform(title.begin(), title.end(), title.begin(), [](char ch) {
                    if (ch == ':' || ch == '|' || ch == '<' || ch == '>' || ch == '/' || ch == '\\' || ch == '*' ||
                        ch == '?') {
                        return '_';
                    } else {
                        return ch;
                    }
                });
                return fmt::format("[{}] {}", disc.header.productNumber, title);
            } else {
                return fmt::format("[{}]", disc.header.productNumber);
            }
        }
    }

    // Fall back to the disc file name if the serial number isn't available
    std::filesystem::path fileName = state.loadedDiscImagePath.filename().replace_extension("");
    if (fileName.empty()) {
        fileName = "nodisc";
    }

    return fileName;
}

std::filesystem::path SharedContext::GetInternalBackupRAMPath() const {
    if (settings.system.internalBackupRAMPerGame) {
        const std::filesystem::path basePath = profile.GetPath(ProfilePath::BackupMemory) / "games";
        std::filesystem::create_directories(basePath);
        return basePath / fmt::format("bup-int-{}.bin", GetGameFileName());
    } else if (!settings.system.internalBackupRAMImagePath.empty()) {
        return settings.system.internalBackupRAMImagePath;
    } else {
        return profile.GetPath(ProfilePath::PersistentState) / "bup-int.bin";
    }
}

ymir::sys::SystemMemory &SharedContext::SaturnContainer::GetSystemMemory() {
    return instance->mem;
}

ymir::sys::Bus &SharedContext::SaturnContainer::GetMainBus() {
    return instance->mainBus;
}

ymir::sh2::SH2 &SharedContext::SaturnContainer::GetMasterSH2() {
    return instance->masterSH2;
}

ymir::sh2::SH2 &SharedContext::SaturnContainer::GetSlaveSH2() {
    return instance->slaveSH2;
}

ymir::scu::SCU &SharedContext::SaturnContainer::GetSCU() {
    return instance->SCU;
}

ymir::vdp::VDP &SharedContext::SaturnContainer::GetVDP() {
    return instance->VDP;
}

ymir::smpc::SMPC &SharedContext::SaturnContainer::GetSMPC() {
    return instance->SMPC;
}

ymir::scsp::SCSP &SharedContext::SaturnContainer::GetSCSP() {
    return instance->SCSP;
}

ymir::cdblock::CDBlock &SharedContext::SaturnContainer::GetCDBlock() {
    return instance->CDBlock;
}

ymir::cart::BaseCartridge &SharedContext::SaturnContainer::GetCartridge() {
    return instance->GetCartridge();
}

bool SharedContext::SaturnContainer::IsSlaveSH2Enabled() const {
    return instance->slaveSH2Enabled;
}

void SharedContext::SaturnContainer::SetSlaveSH2Enabled(bool enabled) {
    instance->slaveSH2Enabled = enabled;
}

bool SharedContext::SaturnContainer::IsDebugTracingEnabled() const {
    return instance->IsDebugTracingEnabled();
}

ymir::XXH128Hash SharedContext::SaturnContainer::GetIPLHash() const {
    return instance->GetIPLHash();
}

ymir::XXH128Hash SharedContext::SaturnContainer::GetDiscHash() const {
    return instance->GetDiscHash();
}

ymir::core::Configuration &SharedContext::SaturnContainer::GetConfiguration() {
    return instance->configuration;
}

} // namespace app
