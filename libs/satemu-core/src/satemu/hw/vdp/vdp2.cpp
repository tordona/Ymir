#include <satemu/hw/vdp/vdp2.hpp>

namespace satemu::vdp2 {

VDP2::VDP2() {
    Reset(true);
}

void VDP2::Reset(bool hard) {
    m_VRAM.fill(0);
    m_CRAM.fill(0);
    TVMD.u16 = 0x0;
    RAMCTL.u16 = 0x0;
    VRSIZE.u16 = 0x0;
    CYCA0.u32 = 0x0;
    CYCA1.u32 = 0x0;
    CYCB0.u32 = 0x0;
    CYCB1.u32 = 0x0;
    BGON.u16 = 0x0;
    MZCTL.u16 = 0x0;
    SFSEL.u16 = 0x0;
    SFCODE.u16 = 0x0;
    CHCTLA.u16 = 0x0;
    CHCTLB.u16 = 0x0;
    BMPNA.u16 = 0x0;
    BMPNB.u16 = 0x0;
    PNCN0.u16 = 0x0;
    PNCN1.u16 = 0x0;
    PNCN2.u16 = 0x0;
    PNCN3.u16 = 0x0;
    PNCR.u16 = 0x0;
    MPOFN.u16 = 0x0;
    MPN0.u32 = 0x0;
    MPN1.u32 = 0x0;
    MPN2.u32 = 0x0;
    MPN3.u32 = 0x0;
    MPRA.ABCDEFGH.u64 = 0x0;
    MPRA.IJKLMNOP.u64 = 0x0;
    MPRB.ABCDEFGH.u64 = 0x0;
    MPRB.IJKLMNOP.u64 = 0x0;
    SCN0.u64 = 0x0;
    ZMN0.u64 = 0x0;
    SCN1.u64 = 0x0;
    ZMN1.u64 = 0x0;
    SCN2.u32 = 0x0;
    SCN3.u32 = 0x0;
    ZMCTL.u16 = 0x0;
    SCRCTL.u16 = 0x0;
    VCSTA.u32 = 0x0;
    LSTA0.u32 = 0x0;
    LSTA1.u32 = 0x0;
    LCTA.u32 = 0x0;
    RPMD.u16 = 0x0;
    RPRCTL.u16 = 0x0;
    KTCTL.u16 = 0x0;
    KTAOF.u16 = 0x0;
    OVPNRA = 0x0;
    OVPNRB = 0x0;
    RPTA.u32 = 0x0;
}

} // namespace satemu::vdp2
