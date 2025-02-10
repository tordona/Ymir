#include <satemu/sys/saturn.hpp>

#include <bit>

namespace satemu {

Saturn::Saturn()
    : SH2(SCU, SMPC)
    , SCU(m_scheduler, VDP, SCSP, CDBlock, SH2)
    , VDP(m_scheduler, SCU)
    , SMPC(m_scheduler, *this)
    , SCSP(m_scheduler, SCU)
    , CDBlock(m_scheduler, SCU, SCSP) {

    Reset(true);
}

void Saturn::Reset(bool hard) {
    if (hard) {
        SetClockRatios(false);

        m_scheduler.Reset();
    }

    SH2.Reset(hard);
    SCU.Reset(hard);
    VDP.Reset(hard);
    SMPC.Reset(hard);
    SCSP.Reset(hard);
    CDBlock.Reset(hard);
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

void Saturn::RunFrame() {
    // Use the last line phase as reference to give some leeway if we overshoot the target cycles
    while (VDP.InLastLinePhase()) {
        Step();
    }
    while (!VDP.InLastLinePhase()) {
        Step();
    }
}

void Saturn::Step() {
    static constexpr uint64 kMaxStep = 64;

    const uint64 cycles = std::min<uint64>(m_scheduler.RemainingCount(), kMaxStep);

    SH2.master.Advance(cycles);
    if (SH2.slaveEnabled) {
        SH2.slave.Advance(cycles);
    }
    SCU.Advance(cycles);
    VDP.Advance(cycles);

    // SCSP+M68K and CD block are ticked by the scheduler

    // TODO: advance SMPC
    /*m_smpcCycles += cycles * 2464;
    const uint64 smpcCycleCount = m_smpcCycles / 17640;
    if (smpcCycleCount > 0) {
        m_smpcCycles -= smpcCycleCount * 17640;
        SMPC.Advance(smpcCycleCount);
    }*/

    m_scheduler.Advance(cycles);
}

void Saturn::SetClockRatios(bool clock352) {
    // TODO: PAL flag
    SCSP.SetClockRatios(clock352, false);
    CDBlock.SetClockRatios(clock352, false);
}

} // namespace satemu
