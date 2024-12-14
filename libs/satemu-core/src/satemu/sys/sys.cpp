#include <satemu/sys/sys.hpp>

namespace satemu {

Saturn::Saturn()
    : SH2(SCU, SMPC)
    , SCU(VDP, SCSP, CDBlock, SH2)
    , VDP(SCU)
    , SMPC(SCU, SCSP)
    , SCSP(SCU)
    , CDBlock(SCU) {

    Reset(true);
}

void Saturn::Reset(bool hard) {
    SH2.Reset(hard);
    SCU.Reset(hard);
    VDP.Reset(hard);
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
    // We use the fastest clocks as a reference, incrementing the cycle counts of the other clocks by the same amount.
    // When a counter reaches the threshold, a full cycle is completed on that clock.
    //
    // Subcycles for NTSC system:
    //   Clock rate    Step   Actual clock rate (error)
    //   28,636,360    2464   28,636,360 (reference)
    //   22,579,200    3125  ~22,579,197 (error: ~0.000001%)
    //   20,000,000    3528  ~19,999,997 (error: ~0.000001%)
    //    4,000,000   17640   ~3,999,999 (error: ~0.000001%)
    //
    // Subcycles for PAL system:
    //   Clock rate    Step   Actual clock rate (error)
    //   28,437,500    9925   28,437,500 (reference)
    //   22,579,200   12500   22,579,375 (error: ~0.00001%)
    //   20,000,000   14112  ~20,000,155 (error: ~0.00001%)
    //    4,000,000   70560   ~4,000,031 (error: ~0.00001%)

    SH2.master.Step();
    // TODO: step slave SH2 if enabled
    // SH2.slave.Step();

    // TODO: replace with scheduler events
    SCU.Advance(1);

    // TODO: replace with scheduler events
    VDP.Advance(1);

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
