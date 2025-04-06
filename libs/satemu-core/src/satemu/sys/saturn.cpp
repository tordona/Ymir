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
    , VDP(m_scheduler)
    , SMPC(m_scheduler, *this)
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
        [&](const std::vector<core::Region> &regions) { UpdatePreferredRegionOrder(regions); });

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

sys::VideoStandard Saturn::GetVideoStandard() const {
    return m_system.videoStandard;
}

void Saturn::SetVideoStandard(sys::VideoStandard videoStandard) {
    m_system.videoStandard = videoStandard;
    VDP.SetVideoStandard(videoStandard);
    m_system.UpdateClockRatios();
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

void Saturn::LoadDisc(media::Disc &&disc) {
    // Configure area code based on compatible area codes from the disc
    if (configuration.system.autodetectRegion && disc.header.compatAreaCode != media::AreaCode::None) {
        // The area code enum is a bitmap where each bit corresponds to an SMPC area code
        const auto areaCodeVal = static_cast<uint16>(disc.header.compatAreaCode);

        // Pick from the preferred list if possible
        bool hasSelectedAreaCode = false;
        for (auto areaCode : m_preferredRegionOrder) {
            if (BitmaskEnum(disc.header.compatAreaCode).AnyOf(areaCode)) {
                SMPC.SetAreaCode(std::countr_zero(static_cast<uint16>(areaCode)));
                hasSelectedAreaCode = true;
                break;
            }
        }

        // If none of the compatible area codes are in the preferred list, pick the first one available
        if (!hasSelectedAreaCode) {
            const uint8 selectedAreaCode = std::countr_zero<uint16>(areaCodeVal & -areaCodeVal);
            SMPC.SetAreaCode(selectedAreaCode);
        }
    }

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

void Saturn::EnableDebugTracing(bool enable) {
    if (m_systemFeatures.enableDebugTracing && !enable) {
        DetachAllTracers();
    }
    m_systemFeatures.enableDebugTracing = enable;
    UpdateRunFrameFn();
}

void Saturn::EnableSH2CacheEmulation(bool enable) {
    if (!m_systemFeatures.emulateSH2Cache && enable) {
        masterSH2.PurgeCache();
        slaveSH2.PurgeCache();
    }
    m_systemFeatures.emulateSH2Cache = enable;
    UpdateRunFrameFn();
}

template <bool debug, bool enableSH2Cache>
void Saturn::RunFrame() {
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
    m_runFrameFn =
        m_systemFeatures.enableDebugTracing
            ? (m_systemFeatures.emulateSH2Cache ? &Saturn::RunFrame<true, true> : &Saturn::RunFrame<true, false>)
            : (m_systemFeatures.emulateSH2Cache ? &Saturn::RunFrame<false, true> : &Saturn::RunFrame<false, false>);
}

void Saturn::UpdatePreferredRegionOrder(std::span<const core::Region> regions) {
    m_preferredRegionOrder.clear();
    media::AreaCode usedAreaCodes = media::AreaCode::None;
    auto addAreaCode = [&](media::AreaCode areaCode) {
        if (BitmaskEnum(usedAreaCodes).NoneOf(areaCode)) {
            usedAreaCodes |= areaCode;
            m_preferredRegionOrder.push_back(areaCode);
        }
    };

    for (const core::Region region : regions) {
        switch (region) {
        case core::Region::Japan: addAreaCode(media::AreaCode::Japan); break;
        case core::Region::AsiaNTSC: addAreaCode(media::AreaCode::AsiaNTSC); break;
        case core::Region::NorthAmerica: addAreaCode(media::AreaCode::NorthAmerica); break;
        case core::Region::CentralSouthAmericaNTSC: addAreaCode(media::AreaCode::CentralSouthAmericaNTSC); break;
        case core::Region::Korea: addAreaCode(media::AreaCode::Korea); break;
        case core::Region::AsiaPAL: addAreaCode(media::AreaCode::AsiaPAL); break;
        case core::Region::EuropePAL: addAreaCode(media::AreaCode::EuropePAL); break;
        case core::Region::CentralSouthAmericaPAL: addAreaCode(media::AreaCode::CentralSouthAmericaPAL); break;
        }
    }
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
