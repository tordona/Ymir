#pragma once

#include "vdp2_defs.hpp"

#include <satemu/hw/hw_defs.hpp>

#include <satemu/util/bit_ops.hpp>
#include <satemu/util/data_ops.hpp>

#include <fmt/format.h>

#include <array>

namespace satemu {

class VDP2 {
public:
    VDP2();

    void Reset(bool hard);

    // TODO: handle VRSIZE.VRAMSZ in Read/WriteVRAM maybe?

    template <mem_access_type T>
    T ReadVRAM(uint32 address) {
        /*address &= 0x7FFFF;
        T value = util::ReadBE<T>(&m_VRAM[address]);
        fmt::println("{}-bit VDP2 VRAM read from {:05X} = {:X}", sizeof(T) * 8, address, value);
        return value;*/
        return util::ReadBE<T>(&m_VRAM[address & 0x7FFFF]);
    }

    template <mem_access_type T>
    void WriteVRAM(uint32 address, T value) {
        // fmt::println("{}-bit VDP2 VRAM write to {:05X} = {:X}", sizeof(T) * 8, address & 0x7FFFF, value);
        util::WriteBE<T>(&m_VRAM[address & 0x7FFFF], value);
    }

    template <mem_access_type T>
    T ReadCRAM(uint32 address) {
        /*address &= MapCRAMAddress(address);
        T value = util::ReadBE<T>(&m_CRAM[address]);
        fmt::println("{}-bit VDP2 CRAM read from {:03X} = {:X}", sizeof(T) * 8, address, value);
        return value;*/
        return util::ReadBE<T>(&m_CRAM[MapCRAMAddress(address)]);
    }

    template <mem_access_type T>
    void WriteCRAM(uint32 address, T value) {
        address = MapCRAMAddress(address);
        // fmt::println("{}-bit VDP2 CRAM write to {:05X} = {:X}", sizeof(T) * 8, address, value);
        util::WriteBE<T>(&m_CRAM[address], value);
        if (RAMCTL.CRMDn == 0) {
            // fmt::println("   replicated to {:05X}", address ^ 0x800);
            util::WriteBE<T>(&m_CRAM[address ^ 0x800], value);
        }
    }

    template <mem_access_type T>
    T ReadReg(uint32 address) {
        switch (address) {
        case 0x000: return TVMD.u16;
        case 0x002: return EXTEN.u16;
        // case 0x004: return 0; // TODO: TVSTAT
        case 0x006: return VRSIZE.u16;
        // case 0x008: return 0; // TODO: HCNT
        // case 0x00A: return 0; // TODO: VCNT
        case 0x00E: return RAMCTL.u16;
        case 0x010: return CYCA0.L.u16;
        case 0x012: return CYCA0.U.u16;
        case 0x014: return CYCA1.L.u16;
        case 0x016: return CYCA1.U.u16;
        case 0x018: return CYCB0.L.u16;
        case 0x01A: return CYCB0.U.u16;
        case 0x01E: return CYCB1.U.u16;
        case 0x01C: return CYCB1.L.u16;
        default: fmt::println("unhandled {}-bit VDP2 register read from {:03X}", sizeof(T) * 8, address); return 0;
        }
    }

    template <mem_access_type T>
    void WriteReg(uint32 address, T value) {
        switch (address) {
        case 0x000: TVMD.u16 = value & 0x81F7; break;
        case 0x002: EXTEN.u16 = value & 0x0303; break;
        case 0x004: /* TVSTAT is read-only */ break;
        case 0x006: VRSIZE.u16 = value & 0x8000; break;
        case 0x008: /* HCNT is read-only */ break;
        case 0x00A: /* VCNT is read-only */ break;
        case 0x00E: RAMCTL.u16 = value & 0xB3FF; break;
        case 0x010: CYCA0.L.u16 = value; break;
        case 0x012: CYCA0.U.u16 = value; break;
        case 0x014: CYCA1.L.u16 = value; break;
        case 0x016: CYCA1.U.u16 = value; break;
        case 0x018: CYCB0.L.u16 = value; break;
        case 0x01A: CYCB0.U.u16 = value; break;
        case 0x01E: CYCB1.U.u16 = value; break;
        case 0x01C: CYCB1.L.u16 = value; break;
        default:
            fmt::println("unhandled {}-bit VDP2 register write to {:03X} = {:X}", sizeof(T) * 8, address, value);
            break;
        }
    }

private:
    std::array<uint8, kVDP2VRAMSize> m_VRAM; // 4x 128 KiB banks: A0, A1, B0, B1
    std::array<uint8, kCRAMSize> m_CRAM;

    // TODO: consider splitting unions into individual fields for performance

    // 180000   TVMD    TV Screen Mode
    //
    //   bits   r/w  code          description
    //     15   R/W  DISP          TV Screen Display (0=no display, 1=display)
    //   14-9   R    -             Reserved, must be zero
    //      8   R/W  BDCLMD        Border Color Mode (0=black, 1=back screen)
    //    7-6   R/W  LSMD1-0       Interlace Mode
    //                               00 (0) = Non-Interlace
    //                               01 (1) = (Forbidden)
    //                               10 (2) = Single-density interlace
    //                               11 (3) = Double-density interlace
    //    5-4   R/W  VRESO1-0      Vertical Resolution
    //                               00 (0) = 224 lines (NTSC or PAL)
    //                               01 (1) = 240 lines (NTSC or PAL)
    //                               10 (2) = 256 lines (PAL only)
    //                               11 (3) = (Forbidden)
    //      3   R    -             Reserved, must be zero
    //    2-0   R/W  HRESO2-0      Horizontal Resolution
    //                               000 (0) = 320 pixels - Normal Graphic A (NTSC or PAL)
    //                               001 (1) = 352 pixels - Normal Graphic B (NTSC or PAL)
    //                               010 (2) = 640 pixels - Hi-Res Graphic A (NTSC or PAL)
    //                               011 (3) = 704 pixels - Hi-Res Graphic B (NTSC or PAL)
    //                               100 (4) = 320 pixels - Exclusive Normal Graphic A (31 KHz monitor)
    //                               101 (5) = 352 pixels - Exclusive Normal Graphic B (Hi-Vision monitor)
    //                               110 (4) = 640 pixels - Exclusive Hi-Res Graphic A (31 KHz monitor)
    //                               111 (5) = 704 pixels - Exclusive Hi-Res Graphic B (Hi-Vision monitor)
    union TVMD_t {
        uint16 u16;
        struct {
            uint16 HRESOn : 3;
            uint16 _rsvd3 : 1;
            uint16 VRESOn : 2;
            uint16 LSMDn : 2;
            uint16 BDCLMD : 1;
            uint16 _rsvd9_14 : 6;
            uint16 DISP : 1;
        };
    } TVMD;

    // 180002   EXTEN   External Signal Enable
    //
    //   bits   r/w  code          description
    //  15-10   R    -             Reserved, must be zero
    //      9   R/W  EXLTEN        External Latch Enable (0=on read, 1=on external signal)
    //      8   R/W  EXSYEN        External Sync Enable (0=disable, 1=enable)
    //    7-2   R    -             Reserved, must be zero
    //      1   R/W  DASEL         Display Area Select (0=selected area, 1=full screen)
    //      0   R/W  EXBGEN        External BG Enable (0=disable, 1=enable)
    union EXTEN_t {
        uint16 u16;
        struct {
            uint16 EXBGEN : 1;
            uint16 DASEL : 1;
            uint16 _rsvd2_7 : 6;
            uint16 EXSYEN : 1;
            uint16 EXLTEN : 1;
            uint16 _rsvd10_15 : 6;
        };
    } EXTEN;

    // 180006   VRSIZE  VRAM Size
    //
    //   bits   r/w  code          description
    //     15   R/W  VRAMSZ        VRAM Size (0=512 KiB, 1=1 MiB)
    //   14-4   R    -             Reserved, must be zero
    //    3-0   R    VER3-0        VDP2 Version Number
    union VRSIZE_t {
        uint16 u16;
        struct {
            uint16 VERn : 4;
            uint16 _rsvd4_14 : 11;
            uint16 VRAMSZ : 1;
        };
    } VRSIZE;

    // 180008   HCNT    H Counter
    //
    //   bits   r/w  code          description
    //  15-10   R    -             Reserved, must be zero
    //    9-0   R    HCT9-0        H Counter Value
    //
    // Notes
    // - Counter layout depends on screen mode:
    //     Normal: bits 8-0 shifted left by 1; HCT0 is invalid
    //     Hi-Res: bits 9-0
    //     Excl. Normal: bits 8-0 (no shift); HCT9 is invalid
    //     Excl. Hi-Res: bits 9-1 shifted right by 1; HCT9 is invalid

    // 18000A   VCNT    V Counter
    //
    //   bits   r/w  code          description
    //  15-10   R    -             Reserved, must be zero
    //    9-0   R    VCT9-0        V Counter Value
    //
    // Notes
    // - Counter layout depends on screen mode:
    //     Exclusive Monitor: bits 9-0
    //     Normal Hi-Res double-density interlace:
    //       bits 8-0 shifted left by 1
    //       bit 0 contains interlaced field (0=odd, 1=even)
    //     All other modes: bits 8-0 shifted left by 1; VCT0 is invalid

    // 18000E   RAMCTL  RAM Control
    //
    //   bits   r/w  code          description
    //     15   R/W  CRKTE         Color RAM Coefficient Table Enable
    //     14   R    -             Reserved, must be zero
    //  13-12   R/W  CRMD1-0       Color RAM Mode
    //                               00 (0) = RGB 5:5:5, 1024 words
    //                               01 (1) = RGB 5:5:5, 2048 words
    //                               10 (2) = RGB 8:8:8, 1024 words
    //                               11 (3) = RGB 8:8:8, 1024 words  (same as mode 2, undocumented)
    //  11-10   R    -             Reserved, must be zero
    //      9   R/W  VRBMD         VRAM-B Mode (0=single partition, 1=two partitions)
    //      8   R/W  VRAMD         VRAM-A Mode (0=single partition, 1=two partitions)
    //    7-6   R/W  RDBSB1(1-0)   Rotation Data Bank Select for VRAM-B1
    //    5-4   R/W  RDBSB0(1-0)   Rotation Data Bank Select for VRAM-B0 (or VRAM-B)
    //    3-2   R/W  RDBSA1(1-0)   Rotation Data Bank Select for VRAM-A1
    //    1-0   R/W  RDBSA0(1-0)   Rotation Data Bank Select for VRAM-A0 (or VRAM-B)
    union RAMCTL_t {
        uint16 u16;
        struct {
            uint16 RDBSA0n : 2;
            uint16 RDBSA1n : 2;
            uint16 RDBSB0n : 2;
            uint16 RDBSB1n : 2;
            uint16 VRAMD : 1;
            uint16 VRBMD : 1;
            uint16 _rsvd10_11 : 2;
            uint16 CRMDn : 2;
            uint16 _rsvd14 : 1;
            uint16 CRKTE : 1;
        };
    } RAMCTL;

    // 180010   CYCA0L  VRAM Cycle Pattern A0 Lower
    //
    //   bits   r/w  code          description
    //  15-12   R/W  VCP0A0(3-0)   VRAM-A0 (or VRAM-A) Timing for T0
    //   11-8   R/W  VCP1A0(3-0)   VRAM-A0 (or VRAM-A) Timing for T1
    //    7-4   R/W  VCP2A0(3-0)   VRAM-A0 (or VRAM-A) Timing for T2
    //    3-0   R/W  VCP3A0(3-0)   VRAM-A0 (or VRAM-A) Timing for T3
    //
    // 180012   CYCA0U  VRAM Cycle Pattern A0 Upper
    //
    //   bits   r/w  code          description
    //  15-12   R/W  VCP4A0(3-0)   VRAM-A0 (or VRAM-A) Timing for T4
    //   11-8   R/W  VCP5A0(3-0)   VRAM-A0 (or VRAM-A) Timing for T5
    //    7-4   R/W  VCP6A0(3-0)   VRAM-A0 (or VRAM-A) Timing for T6
    //    3-0   R/W  VCP7A0(3-0)   VRAM-A0 (or VRAM-A) Timing for T7
    //
    // 180014   CYCA1L  VRAM Cycle Pattern A1 Lower
    //
    //   bits   r/w  code          description
    //  15-12   R/W  VCP0A1(3-0)   VRAM-A1 Timing for T0
    //   11-8   R/W  VCP1A1(3-0)   VRAM-A1 Timing for T1
    //    7-4   R/W  VCP2A1(3-0)   VRAM-A1 Timing for T2
    //    3-0   R/W  VCP3A1(3-0)   VRAM-A1 Timing for T3
    //
    // 180016   CYCA1U  VRAM Cycle Pattern A1 Upper
    //
    //   bits   r/w  code          description
    //  15-12   R/W  VCP4A1(3-0)   VRAM-A1 Timing for T4
    //   11-8   R/W  VCP5A1(3-0)   VRAM-A1 Timing for T5
    //    7-4   R/W  VCP6A1(3-0)   VRAM-A1 Timing for T6
    //    3-0   R/W  VCP7A1(3-0)   VRAM-A1 Timing for T7
    //
    // 180018   CYCB0L  VRAM Cycle Pattern B0 Lower
    //
    //   bits   r/w  code          description
    //  15-12   R/W  VCP0B0(3-0)   VRAM-b0 (or VRAM-B) Timing for T0
    //   11-8   R/W  VCP1B0(3-0)   VRAM-b0 (or VRAM-B) Timing for T1
    //    7-4   R/W  VCP2B0(3-0)   VRAM-b0 (or VRAM-B) Timing for T2
    //    3-0   R/W  VCP3B0(3-0)   VRAM-b0 (or VRAM-B) Timing for T3
    //
    // 18001A   CYCB0U  VRAM Cycle Pattern B0 Upper
    //
    //   bits   r/w  code          description
    //  15-12   R/W  VCP4B0(3-0)   VRAM-B0 (or VRAM-B) Timing for T4
    //   11-8   R/W  VCP5B0(3-0)   VRAM-B0 (or VRAM-B) Timing for T5
    //    7-4   R/W  VCP6B0(3-0)   VRAM-B0 (or VRAM-B) Timing for T6
    //    3-0   R/W  VCP7B0(3-0)   VRAM-B0 (or VRAM-B) Timing for T7
    //
    // 18001C   CYCB1L  VRAM Cycle Pattern B1 Lower
    //
    //   bits   r/w  code          description
    //  15-12   R/W  VCP0B1(3-0)   VRAM-B1 Timing for T0
    //   11-8   R/W  VCP1B1(3-0)   VRAM-B1 Timing for T1
    //    7-4   R/W  VCP2B1(3-0)   VRAM-B1 Timing for T2
    //    3-0   R/W  VCP3B1(3-0)   VRAM-B1 Timing for T3
    //
    // 18001E   CYCB1U  VRAM Cycle Pattern B1 Upper
    //
    //   bits   r/w  code          description
    //  15-12   R/W  VCP4B1(3-0)   VRAM-B1 Timing for T4
    //   11-8   R/W  VCP5B1(3-0)   VRAM-B1 Timing for T5
    //    7-4   R/W  VCP6B1(3-0)   VRAM-B1 Timing for T6
    //    3-0   R/W  VCP7B1(3-0)   VRAM-B1 Timing for T7
    //
    union CYC_t {
        uint32 u32;
        struct {
            union {
                uint16 u16;
                struct {
                    uint16 VCP3n : 4;
                    uint16 VCP2n : 4;
                    uint16 VCP1n : 4;
                    uint16 VCP0n : 4;
                };
            } L;
            union {
                uint16 u16;
                struct {
                    uint16 VCP7n : 4;
                    uint16 VCP6n : 4;
                    uint16 VCP5n : 4;
                    uint16 VCP4n : 4;
                };
            } U;
        };
    } CYCA0, CYCA1, CYCB0, CYCB1;

    // -------------------------------------------------------------------------

    // CRAM has four modes specified by RAMCTL.CRMD. Each mode has some addressing quirks:
    //   0 - writes to the lower half are mirrored to the upper half and vice-versa
    //   1 - no mirroring
    //   2 and 3 - address bits are shuffled as follows:
    //      10 09 08 07 06 05 04 03 02 01 11 00
    //      in short, bits 10-01 are shifted left and bit 11 takes place of bit 01
    uint32 MapCRAMAddress(uint32 address) const {
        if (RAMCTL.CRMDn == 2 || RAMCTL.CRMDn == 3) {
            address =
                bit::extract<0>(address) | (bit::extract<11>(address) << 1u) | (bit::extract<1, 10>(address) << 2u);
        }
        return address;
    }
};

} // namespace satemu
