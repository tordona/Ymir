#include <satemu/sys/sys.hpp>

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
    m_scheduler.Reset();

    SH2.Reset(hard);
    SCU.Reset(hard);
    VDP.Reset(hard);
    SMPC.Reset(hard);
    SCSP.Reset(hard);
    CDBlock.Reset(hard);

    m_scspCycles = 0;
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

void Saturn::RunFrame() {
    // Run until VDP exits top blanking phase
    while (VDP.InTopBlankingPhase()) {
        Step();
    }

    // Run until VDP enters top blanking phase
    while (!VDP.InTopBlankingPhase()) {
        Step();
    }
}

void Saturn::Step() {
    // Clock speeds:
    // - SH-2:
    //   - 320 mode: 26.846591 MHz (NTSC) / 26.660156 MHz (PAL)
    //   - 352 mode: 28.636364 MHz (NTSC) / 28.437500 MHz (PAL)
    // - VDP1, VDP2, SCU share the SH2 clock
    //   - VDP pixel clock is 1/2 on hi-res modes or 1/4 at lo-res modes
    //   - SCU DSP runs at 1/2 clock speed
    // - SCSP: 22.579200 MHz (44100 * 512)
    //   - MC68EC000 runs at 1/2 SCSP clock
    // - CD Block SH1: 20.000000 MHz
    // - SMPC MCU: 4.000000 MHz

    // The listed ratios below are all exact and relative to the primary system clock (SH-2/VDPs/SCU).
    // These ratios are used in the scheduler to accurately schedule events relative to each clock.
    //
    // NTSC system at clock 352 mode:
    //   Clock rate         Ratio       Minimized ratio
    //   28,636,363.64   2464:2464            1:1
    //   22,579,200.00   2464:3125         2464:3125
    //   20,000,000.00   2464:3528           44:63
    //    4,000,000.00   2464:17640          44:315
    //
    // NTSC system at clock 320 mode:
    //   Clock rate         Ratio       Minimized ratio
    //   26,846,590.91   39424:39424          1:1
    //   22,579,200.00   39424:46875      39424:46875
    //   20,000,000.00   39424:52920        704:945
    //    4,000,000.00   39424:264600       704:4725
    //
    // PAL system at clock 352 mode:
    //   Clock rate         Ratio       Minimized ratio
    //   28,437,500.00   32256:32256          1:1
    //   22,579,200.00   32256:40625      32256:40625
    //   20,000,000.00   32256:45864         64:91
    //    4,000,000.00   32256:229320        64:455
    //
    // PAL system at clock 320 mode:
    //   Clock rate         Ratio       Minimized ratio
    //   26,660,156.25   172032:172032        1:1
    //   22,579,200.00   172032:203125   172032:203125
    //   20,000,000.00   172032:229320     1024:1365
    //    4,000,000.00   172032:1146600    1024:6825

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

} // namespace satemu
