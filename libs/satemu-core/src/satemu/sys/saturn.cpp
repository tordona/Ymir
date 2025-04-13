#include <satemu/sys/saturn.hpp>

#include <satemu/util/dev_log.hpp>

#include <bit>

namespace satemu {

namespace grp {

    // -----------------------------------------------------------------------------
    // Dev log groups

    // Hierarchy:
    //
    // system
    // bus

    struct system {
        static constexpr bool enabled = true;
        static constexpr devlog::Level level = devlog::level::debug;
        static constexpr std::string_view name = "System";
    };

    struct bus {
        static constexpr bool enabled = true;
        static constexpr devlog::Level level = devlog::level::debug;
        static constexpr std::string_view name = "Bus";
    };

} // namespace grp

Saturn::Saturn()
    : masterSH2(mainBus, true, m_systemFeatures)
    , slaveSH2(mainBus, false, m_systemFeatures)
    , SCU(m_scheduler, mainBus)
    , VDP(m_scheduler, configuration)
    , SMPC(m_scheduler, *this, configuration.rtc)
    , SCSP(m_scheduler, configuration.audio)
    , CDBlock(m_scheduler) {

    mainBus.MapNormal(
        0x000'0000, 0x7FF'FFFF, nullptr,
        [](uint32 address, void *) -> uint8 {
            devlog::debug<grp::bus>("Unhandled 8-bit main bus read from {:07X}", address);
            return 0;
        },
        [](uint32 address, void *) -> uint16 {
            devlog::debug<grp::bus>("Unhandled 16-bit main bus read from {:07X}", address);
            return 0;
        },
        [](uint32 address, void *) -> uint32 {
            devlog::debug<grp::bus>("Unhandled 32-bit main bus read from {:07X}", address);
            return 0;
        },
        [](uint32 address, uint8 value, void *) {
            devlog::debug<grp::bus>("Unhandled 8-bit main bus write to {:07X} = {:02X}", address, value);
        },
        [](uint32 address, uint16 value, void *) {
            devlog::debug<grp::bus>("Unhandled 16-bit main bus write to {:07X} = {:04X}", address, value);
        },
        [](uint32 address, uint32 value, void *) {
            devlog::debug<grp::bus>("Unhandled 32-bit main bus write to {:07X} = {:07X}", address, value);
        });

    masterSH2.MapCallbacks(SCU.CbAckExtIntr);
    slaveSH2.MapCallbacks(SCU.CbAckExtIntr);
    SCU.MapCallbacks(masterSH2.CbExtIntr, slaveSH2.CbExtIntr);
    VDP.MapCallbacks(SCU.CbTriggerHBlankIN, SCU.CbTriggerVBlankIN, SCU.CbTriggerVBlankOUT, SCU.CbTriggerSpriteDrawEnd,
                     SMPC.CbTriggerOptimizedINTBACKRead);
    SMPC.MapCallbacks(SCU.CbTriggerSystemManager);
    SCSP.MapCallbacks(SCU.CbTriggerSoundRequest);
    CDBlock.MapCallbacks(SCU.CbTriggerExtIntr0, SCSP.CbCDDASector);

    m_system.AddClockSpeedChangeCallback(SCSP.CbClockSpeedChange);
    m_system.AddClockSpeedChangeCallback(SMPC.CbClockSpeedChange);
    m_system.AddClockSpeedChangeCallback(CDBlock.CbClockSpeedChange);

    mem.MapMemory(mainBus);
    masterSH2.MapMemory(mainBus);
    slaveSH2.MapMemory(mainBus);
    SCU.MapMemory(mainBus);
    VDP.MapMemory(mainBus);
    SMPC.MapMemory(mainBus);
    SCSP.MapMemory(mainBus);
    CDBlock.MapMemory(mainBus);

    m_systemFeatures.enableDebugTracing = false;
    m_systemFeatures.emulateSH2Cache = false;
    UpdateRunFrameFn();

    configuration.system.preferredRegionOrder.Observe(
        [&](const std::vector<config::sys::Region> &regions) { UpdatePreferredRegionOrder(regions); });
    configuration.system.emulateSH2Cache.Observe([&](bool enabled) { UpdateSH2CacheEmulation(enabled); });
    configuration.system.videoStandard.Observe(
        [&](config::sys::VideoStandard videoStandard) { UpdateVideoStandard(videoStandard); });

    Reset(true);
}

void Saturn::Reset(bool hard) {
    if (hard) {
        m_system.clockSpeed = sys::ClockSpeed::_320;
        m_system.UpdateClockRatios();

        m_scheduler.Reset();
    }

    masterSH2.Reset(hard);
    slaveSH2.Reset(hard);
    slaveSH2Enabled = false;

    SCU.Reset(hard);
    VDP.Reset(hard);
    SMPC.Reset(hard);
    SCSP.Reset(hard);
    CDBlock.Reset(hard);
}

void Saturn::FactoryReset() {
    SMPC.FactoryReset();
    Reset(true);
}

sys::ClockSpeed Saturn::GetClockSpeed() const {
    return m_system.clockSpeed;
}

void Saturn::SetClockSpeed(sys::ClockSpeed clockSpeed) {
    m_system.clockSpeed = clockSpeed;
    m_system.UpdateClockRatios();
}

const sys::ClockRatios &Saturn::GetClockRatios() const {
    return m_system.GetClockRatios();
}

void Saturn::LoadIPL(std::span<uint8, sys::kIPLSize> ipl) {
    mem.LoadIPL(ipl);
}

Hash128 Saturn::GetIPLHash() const {
    return mem.GetIPLHash();
}

Hash128 Saturn::GetDiscHash() const {
    return CDBlock.GetDiscHash();
}

void Saturn::LoadDisc(media::Disc &&disc) {
    // Configure area code based on compatible area codes from the disc
    AutodetectRegion(disc.header.compatAreaCode);
    CDBlock.LoadDisc(std::move(disc));
}

void Saturn::EjectDisc() {
    CDBlock.EjectDisc();
}

void Saturn::OpenTray() {
    CDBlock.OpenTray();
}

void Saturn::CloseTray() {
    CDBlock.CloseTray();
}

bool Saturn::IsTrayOpen() const {
    return CDBlock.IsTrayOpen();
}

void Saturn::AutodetectRegion(media::AreaCode areaCodes) {
    if (!configuration.system.autodetectRegion) {
        return;
    }
    if (areaCodes == media::AreaCode::None) {
        return;
    }

    const uint8 currAreaCode = SMPC.GetAreaCode();

    // The area code enum is a bitmap where each bit corresponds to an SMPC area code
    const auto areaCodeVal = static_cast<uint16>(areaCodes);

    // Pick from the preferred list if possible or use the first one found
    uint8 selectedAreaCode = std::countr_zero<uint16>(areaCodeVal & -areaCodeVal);
    for (auto areaCode : m_preferredRegionOrder) {
        if (BitmaskEnum(areaCodes).AnyOf(areaCode)) {
            selectedAreaCode = std::countr_zero(static_cast<uint16>(areaCode));
            break;
        }
    }

    // Apply configuration and hard reset system if changed
    SMPC.SetAreaCode(selectedAreaCode);
    if (selectedAreaCode != currAreaCode) {
        Reset(true);
    }
}

void Saturn::EnableDebugTracing(bool enable) {
    if (m_systemFeatures.enableDebugTracing && !enable) {
        DetachAllTracers();
    }
    m_systemFeatures.enableDebugTracing = enable;
    UpdateRunFrameFn();
}

void Saturn::SaveState(state::State &state) const {
    m_scheduler.SaveState(state.scheduler);
    m_system.SaveState(state.system);
    mem.SaveState(state.system);
    state.system.slaveSH2Enabled = slaveSH2Enabled;
    masterSH2.SaveState(state.msh2);
    slaveSH2.SaveState(state.ssh2);
    SCU.SaveState(state.scu);
    SMPC.SaveState(state.smpc);
    VDP.SaveState(state.vdp);
    SCSP.SaveState(state.scsp);
    CDBlock.SaveState(state.cdblock);
}

bool Saturn::LoadState(const state::State &state) {
    if (!m_scheduler.ValidateState(state.scheduler)) {
        return false;
    }
    if (!m_system.ValidateState(state.system)) {
        return false;
    }
    if (!mem.ValidateState(state.system)) {
        return false;
    }
    if (!masterSH2.ValidateState(state.msh2)) {
        return false;
    }
    if (!slaveSH2.ValidateState(state.ssh2)) {
        return false;
    }
    if (!SCU.ValidateState(state.scu)) {
        return false;
    }
    if (!SMPC.ValidateState(state.smpc)) {
        return false;
    }
    if (!VDP.ValidateState(state.vdp)) {
        return false;
    }
    if (!SCSP.ValidateState(state.scsp)) {
        return false;
    }
    if (!CDBlock.ValidateState(state.cdblock)) {
        return false;
    }

    m_scheduler.LoadState(state.scheduler);
    m_system.LoadState(state.system);
    mem.LoadState(state.system);
    slaveSH2Enabled = state.system.slaveSH2Enabled;
    masterSH2.LoadState(state.msh2);
    slaveSH2.LoadState(state.ssh2);
    SCU.LoadState(state.scu);
    SMPC.LoadState(state.smpc);
    VDP.LoadState(state.vdp);
    SCSP.LoadState(state.scsp);
    CDBlock.LoadState(state.cdblock);

    return true;
}

template <bool debug, bool enableSH2Cache>
void Saturn::RunFrameImpl() {
    // Use the last line phase as reference to give some leeway if we overshoot the target cycles
    while (VDP.InLastLinePhase()) {
        Run<debug, enableSH2Cache>();
    }
    while (!VDP.InLastLinePhase()) {
        Run<debug, enableSH2Cache>();
    }
}

template <bool debug, bool enableSH2Cache>
void Saturn::Run() {
    static constexpr uint64 kMaxStep = 64;

    const uint64 cycles = std::min<uint64>(m_scheduler.RemainingCount(), kMaxStep);

    masterSH2.Advance<debug, enableSH2Cache>(cycles);
    if (slaveSH2Enabled) {
        slaveSH2.Advance<debug, enableSH2Cache>(cycles);
    }
    SCU.Advance<debug>(cycles);
    VDP.Advance<debug>(cycles);

    // SCSP+M68K and CD block are ticked by the scheduler

    // TODO: advance SMPC
    /*m_smpcCycles += cycles * 2464;
    const uint64 smpcCycleCount = m_smpcCycles / 17640;
    if (smpcCycleCount > 0) {
        m_smpcCycles -= smpcCycleCount * 17640;
        SMPC.Advance<debug>(smpcCycleCount);
    }*/

    m_scheduler.Advance(cycles);
}

void Saturn::UpdateRunFrameFn() {
    m_runFrameFn = m_systemFeatures.enableDebugTracing
                       ? (m_systemFeatures.emulateSH2Cache ? &Saturn::RunFrameImpl<true, true>
                                                           : &Saturn::RunFrameImpl<true, false>)
                       : (m_systemFeatures.emulateSH2Cache ? &Saturn::RunFrameImpl<false, true>
                                                           : &Saturn::RunFrameImpl<false, false>);
}

void Saturn::UpdatePreferredRegionOrder(std::span<const config::sys::Region> regions) {
    m_preferredRegionOrder.clear();
    media::AreaCode usedAreaCodes = media::AreaCode::None;
    auto addAreaCode = [&](media::AreaCode areaCode) {
        if (BitmaskEnum(usedAreaCodes).NoneOf(areaCode)) {
            usedAreaCodes |= areaCode;
            m_preferredRegionOrder.push_back(areaCode);
        }
    };

    using Region = config::sys::Region;
    for (const Region region : regions) {
        switch (region) {
        case Region::Japan: addAreaCode(media::AreaCode::Japan); break;
        case Region::AsiaNTSC: addAreaCode(media::AreaCode::AsiaNTSC); break;
        case Region::NorthAmerica: addAreaCode(media::AreaCode::NorthAmerica); break;
        case Region::CentralSouthAmericaNTSC: addAreaCode(media::AreaCode::CentralSouthAmericaNTSC); break;
        case Region::Korea: addAreaCode(media::AreaCode::Korea); break;
        case Region::AsiaPAL: addAreaCode(media::AreaCode::AsiaPAL); break;
        case Region::EuropePAL: addAreaCode(media::AreaCode::EuropePAL); break;
        case Region::CentralSouthAmericaPAL: addAreaCode(media::AreaCode::CentralSouthAmericaPAL); break;
        }
    }
}

void Saturn::UpdateSH2CacheEmulation(bool enabled) {
    if (!m_systemFeatures.emulateSH2Cache && enabled) {
        masterSH2.PurgeCache();
        slaveSH2.PurgeCache();
    }
    m_systemFeatures.emulateSH2Cache = enabled;
    UpdateRunFrameFn();
}

void Saturn::UpdateVideoStandard(config::sys::VideoStandard videoStandard) {
    m_system.videoStandard = videoStandard;
    m_system.UpdateClockRatios();
}

bool Saturn::GetNMI() const {
    return masterSH2.GetNMI();
}

void Saturn::RaiseNMI() {
    masterSH2.SetNMI();
}

void Saturn::EnableAndResetSlaveSH2() {
    slaveSH2Enabled = true;
    slaveSH2.Reset(true);
}

void Saturn::DisableSlaveSH2() {
    slaveSH2Enabled = false;
}

void Saturn::EnableAndResetM68K() {
    SCSP.SetCPUEnabled(true);
}

void Saturn::DisableM68K() {
    SCSP.SetCPUEnabled(false);
}

void Saturn::SoftResetSystem() {
    Reset(false);
}

void Saturn::ClockChangeSoftReset() {
    VDP.Reset(false);
    SCU.Reset(false);
    SCSP.Reset(false);
}

} // namespace satemu
