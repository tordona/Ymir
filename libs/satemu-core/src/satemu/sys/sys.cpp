#include <satemu/sys/sys.hpp>

namespace satemu {

Saturn::Saturn()
    : SH2(SCU, SMPC)
    , SCU(VDP1, VDP2, SCSP, CDBlock, SH2.master)
    , VDP2(SCU)
    , SMPC(SCU, SCSP)
    , SCSP(SCU)
    , CDBlock(SCU) {

    Reset(true);
}

void Saturn::Reset(bool hard) {
    SH2.Reset(hard);
    SCU.Reset(hard);
    VDP1.Reset(hard);
    VDP2.Reset(hard);
    SMPC.Reset(hard);
    SCSP.Reset(hard);
    CDBlock.Reset(hard);

    m_scspCycles = 0;
    m_cdbCycles = 0;
}

void Saturn::LoadIPL(std::span<uint8, sh2::kIPLSize> ipl) {
    SH2.bus.LoadIPL(ipl);
}

void Saturn::LoadDisc(media::Disc &&disc) {
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

void Saturn::Step() {
    // Clock speeds:
    // - SH2: 28.63636 MHz on NTSC systems or 28.43750 MHz on PAL systems
    // - VDP1, VDP2, SCU share the SH2 clock
    //   - VDP pixel clock is 1/2 on hi-res modes or 1/4 at lo-res modes
    //   - SCU DSP runs at 1/2 clock speed
    // - SCSP: 22.57920 MHz (44100 * 512)
    //   - MC68EC000 runs at 1/2 SCSP clock
    // - CD Block SH1: 20.00000 MHz
    // - SMPC MCU: 4.00000 MHz

    // Subcycle counting is needed to accurately emulate the timings between these components.
    // We count cycles for each clock separately, incrementing by the smallest subcycle count.
    // When a counter reaches the step threshold, a full cycle is completed on that component.
    //
    // Subcycles for NTSC system:
    //   Clock        Subcycles per step
    //   28,636,360    2464  (reference)
    //   22,579,200    3125  (error: ~0.000001%)
    //   20,000,000    3528  (error: ~0.000001%)
    //    4,000,000   14112  (error: ~0.000001%)
    //
    // Subcycles for PAL system:
    //   Clock        Subcycles per step
    //   28,437,500    9925   (reference)
    //   22,579,200   12500   (error: ~0.00001%)
    //   20,000,000   14112   (error: ~0.00001%)
    //    4,000,000   56448   (error: ~0.00001%)

    SH2.master.Step();
    // TODO: step slave SH2 if enabled
    // SH2.slave.Step();

    // TODO: replace with scheduler events
    VDP2.Advance(1);

    m_scspCycles += 2464;
    if (m_scspCycles >= 3125) {
        m_scspCycles -= 3125;
        // TODO: replace with scheduler events
        SCSP.Advance(1);
    }

    m_cdbCycles += 2464;
    if (m_cdbCycles >= 3528) {
        m_cdbCycles -= 3528;
        // TODO: replace with scheduler events
        CDBlock.Advance(1);

        // TODO: advance SMPC once every 5 ticks
    }
}

} // namespace satemu
