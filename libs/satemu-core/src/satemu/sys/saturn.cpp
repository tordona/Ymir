#include <satemu/sys/saturn.hpp>

#include <bit>

namespace satemu {

Saturn::Saturn()
    : masterSH2(mainBus, true)
    , slaveSH2(mainBus, false)
    , SCU(m_scheduler, mainBus)
    , VDP(m_scheduler)
    , SMPC(m_scheduler, *this)
    , SCSP(m_scheduler)
    , CDBlock(m_scheduler) {

    masterSH2.SetExternalInterruptAcknowledgeCallback(SCU.CbAckExtIntr);
    slaveSH2.SetExternalInterruptAcknowledgeCallback(SCU.CbAckExtIntr);

    SCU.SetExternalInterruptCallbacks(masterSH2.CbExtIntr, slaveSH2.CbExtIntr);

    VDP.SetInterruptCallbacks(SCU.CbTriggerHBlankIN, SCU.CbTriggerVBlankIN, SCU.CbTriggerVBlankOUT,
                              SCU.CbTriggerSpriteDrawEnd, SMPC.CbTriggerOptimizedINTBACKRead);

    SMPC.SetSystemManagerInterruptCallback(SCU.CbTriggerSystemManager);

    SCSP.SetTriggerSoundRequestInterruptCallback(SCU.CbTriggerSoundRequest);

    CDBlock.SetTriggerExternalInterrupt0Callback(SCU.CbTriggerExtIntr0);
    CDBlock.SetCDDASectorCallback(SCSP.CbCDDASector);

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

void Saturn::LoadIPL(std::span<uint8, sys::kIPLSize> ipl) {
    mem.LoadIPL(ipl);
}

void Saturn::LoadDisc(media::Disc &&disc) {
    // Configure area code based on compatible area codes from the disc
    // TODO: make area code autoconfiguration optional
    if (disc.header.compatAreaCode != media::AreaCode::None) {
        // The area code enum is a bitmap where each bit corresponds to an SMPC area code
        const auto areaCodeVal = static_cast<uint16>(disc.header.compatAreaCode);

        // Pick from the preferred list if possible
        bool hasSelectedAreaCode = false;
        // TODO: make preferred order configurable
        static media::AreaCode kPreferredOrder[] = {media::AreaCode::NorthAmerica, media::AreaCode::Japan};
        for (auto areaCode : kPreferredOrder) {
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

template <bool debug>
void Saturn::RunFrame() {
    // Use the last line phase as reference to give some leeway if we overshoot the target cycles
    while (VDP.InLastLinePhase()) {
        Run<debug>();
    }
    while (!VDP.InLastLinePhase()) {
        Run<debug>();
    }
}

template void Saturn::RunFrame<false>();
template void Saturn::RunFrame<true>();

template <bool debug>
void Saturn::Run() {
    static constexpr uint64 kMaxStep = 64;

    const uint64 cycles = std::min<uint64>(m_scheduler.RemainingCount(), kMaxStep);

    masterSH2.Advance<debug>(cycles);
    if (slaveSH2Enabled) {
        slaveSH2.Advance<debug>(cycles);
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
