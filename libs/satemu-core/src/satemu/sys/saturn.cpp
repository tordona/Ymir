#include <satemu/sys/saturn.hpp>

#include <bit>

namespace satemu {

Saturn::Saturn()
    : SH2(SCU, SMPC)
    , SCU(m_scheduler, SH2)
    , VDP(m_scheduler, SCU, SMPC)
    , SMPC(m_system, m_scheduler, *this)
    , SCSP(m_system, m_scheduler, SCU)
    , CDBlock(m_system, m_scheduler, SCU, SCSP) {

    SCU.MapMemory(SH2.bus);
    VDP.MapMemory(SH2.bus);
    SMPC.MapMemory(SH2.bus);
    SCSP.MapMemory(SH2.bus);
    CDBlock.MapMemory(SH2.bus);

    Reset(true);
}

void Saturn::Reset(bool hard) {
    if (hard) {
        m_system.clockSpeed = sys::ClockSpeed::_320;
        UpdateClockRatios();

        m_scheduler.Reset();
    }

    SH2.Reset(hard);
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
    UpdateClockRatios();
}

sys::ClockSpeed Saturn::GetClockSpeed() const {
    return m_system.clockSpeed;
}

void Saturn::SetClockSpeed(sys::ClockSpeed clockSpeed) {
    m_system.clockSpeed = clockSpeed;
    UpdateClockRatios();
}

void Saturn::LoadIPL(std::span<uint8, sh2::kIPLSize> ipl) {
    SH2.bus.LoadIPL(ipl);
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
        Step<debug>();
    }
    while (!VDP.InLastLinePhase()) {
        Step<debug>();
    }
}

template void Saturn::RunFrame<false>();
template void Saturn::RunFrame<true>();

template <bool debug>
void Saturn::Step() {
    static constexpr uint64 kMaxStep = 64;

    const uint64 cycles = std::min<uint64>(m_scheduler.RemainingCount(), kMaxStep);

    SH2.master.Advance<debug>(cycles);
    if (SH2.slaveEnabled) {
        SH2.slave.Advance<debug>(cycles);
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

void Saturn::UpdateClockRatios() {
    SCSP.UpdateClockRatios();
    CDBlock.UpdateClockRatios();
}

} // namespace satemu
