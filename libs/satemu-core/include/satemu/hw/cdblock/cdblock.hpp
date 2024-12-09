#pragma once

#include "cdblock_defs.hpp"

#include <satemu/hw/hw_defs.hpp>

#include <fmt/format.h>

#include <array>
#include <type_traits>

// -----------------------------------------------------------------------------
// Forward declarations

namespace satemu::scu {

class SCU;

} // namespace satemu::scu

// -----------------------------------------------------------------------------

namespace satemu::cdblock {

class CDBlock {
public:
    CDBlock(scu::SCU &scu);

    void Reset(bool hard);

    void Advance(uint64 cycles);

    // TODO: handle 8-bit and 32-bit accesses properly

    template <mem_access_type T>
    T Read(uint32 address) {
        T value = ReadImpl<T>(address);
        fmt::println("{}-bit CD Block read from {:02X} = {:X}", sizeof(T) * 8, address, value);
        return value;
    }

    template <mem_access_type T>
    T ReadImpl(uint32 address) {
        // TODO: implement properly; we're just stubbing the CDBLOCK init sequence here
        switch (address) {
        case 0x08: // return m_HIRQ;
            // MEGA HACK to get past the boot sequence
            return std::is_same_v<T, uint8> ? (0x601 >> (((address & 1) ^ 1) * 8)) : 0x601;
        case 0x0C: return m_HIRQMASK;
        case 0x18: return m_CR[0];
        case 0x1C: return m_CR[1];
        case 0x20: return m_CR[2];
        case 0x24: {
            uint16 result = m_CR[3];

            // MEGA HACK! replace with a blank periodic report to get past the boot sequence
            // TODO: implement periodic CD status reporting *properly*
            ReportCDStatus();

            return result;
        }
        default: fmt::println("unhandled {}-bit CD Block read from {:02X}", sizeof(T) * 8, address); return 0;
        }
    }

    template <mem_access_type T>
    void Write(uint32 address, T value) {
        fmt::println("{}-bit CD Block write to {:02X} = {:X}", sizeof(T) * 8, address, value);
        switch (address) {
        case 0x08:
            m_HIRQ &= value;
            UpdateInterrupts();
            break;
        case 0x0C:
            m_HIRQMASK = value;
            UpdateInterrupts();
            break;

        default: fmt::println("unhandled {}-bit CD Block write to {:02X} = {:X}", sizeof(T) * 8, address, value); break;
        }
    }

private:
    scu::SCU &m_scu;

    alignas(uint64) std::array<uint16, 4> m_CR;

    // -------------------------------------------------------------------------
    // Disc/drive state

    struct Status {
        // Status code, one of kStatusCode* constants and kStatusFlag* flags, or kStatusReject.
        // kStatusFlagPeriodic and kStatusFlagWait are mutually exclusive.
        uint8 statusCode;

        uint32 frameAddress;
        uint8 flagsRepeat; // 7-4: flags, 3-0: repeat count (0x0 to 0xE)
        uint8 controlADR;
        uint8 track;
        uint8 index;
    } m_status;

    // -------------------------------------------------------------------------
    // Interrupts

    uint16 m_HIRQ;
    uint16 m_HIRQMASK;

    void SetInterrupt(uint16 bits);
    void UpdateInterrupts();

    // -------------------------------------------------------------------------
    // Status reports

    void ReportCDStatus();

    // -------------------------------------------------------------------------
    // Commands

    void ProcessCommand();

    void CmdGetStatus();                    // 0x00
    void CmdGetHardwareInfo();              // 0x01
    void CmdGetTOC();                       // 0x02
    void CmdGetSessionInfo();               // 0x03
    void CmdInitializeCDSystem();           // 0x04
    void CmdOpenTray();                     // 0x05
    void CmdEndDataTransfer();              // 0x06
    void CmdPlayDisc();                     // 0x10
    void CmdSeekDisc();                     // 0x11
    void CmdScanDisc();                     // 0x12
    void CmdGetSubcodeQ_RW();               // 0x20
    void CmdSetCDDeviceConnection();        // 0x30
    void CmdGetCDDeviceConnection();        // 0x31
    void CmdGetLastBufferDest();            // 0x32
    void CmdSetFilterRange();               // 0x40
    void CmdGetFilterRange();               // 0x41
    void CmdSetFilterSubheaderConditions(); // 0x42
    void CmdGetFilterSubheaderConditions(); // 0x43
    void CmdSetFilterMode();                // 0x44
    void CmdGetFilterMode();                // 0x45
    void CmdSetFilterConnection();          // 0x46
    void CmdGetFilterConnection();          // 0x47
    void CmdResetSelector();                // 0x48
    void CmdGetBufferSize();                // 0x50
    void CmdGetSectorNumber();              // 0x51
    void CmdCalculateActualSize();          // 0x52
    void CmdGetActualSize();                // 0x53
    void CmdGetSectorInfo();                // 0x54
    void CmdExecuteFADSearch();             // 0x55
    void CmdGetFADSearchResults();          // 0x56
    void CmdSetSectorLength();              // 0x60
    void CmdGetSectorData();                // 0x61
    void CmdDeleteSectorData();             // 0x62
    void CmdGetThenDeleteSectorData();      // 0x63
    void CmdPutSectorData();                // 0x64
    void CmdCopySectorData();               // 0x65
    void CmdMoveSectorData();               // 0x66
    void CmdGetCopyError();                 // 0x67
    void CmdChangeDirectory();              // 0x70
    void CmdReadDirectory();                // 0x71
    void CmdGetFileSystemScope();           // 0x72
    void CmdGetFileInfo();                  // 0x73
    void CmdReadFile();                     // 0x74
    void CmdAbortFile();                    // 0x75

    void CmdMpegGetStatus();         // 0x90
    void CmdMpegGetInterrupt();      // 0x91
    void CmdMpegSetInterruptMask();  // 0x92
    void CmdMpegInit();              // 0x93
    void CmdMpegSetMode();           // 0x94
    void CmdMpegPlay();              // 0x95
    void CmdMpegSetDecodingMethod(); // 0x96
    void CmdMpegSetConnection();     // 0x9A
    void CmdMpegGetConnection();     // 0x9B
    void CmdMpegSetStream();         // 0x9D
    void CmdMpegGetStream();         // 0x9E
    void CmdMpegDisplay();           // 0xA0
    void CmdMpegSetWindow();         // 0xA1
    void CmdMpegSetBorderColor();    // 0xA2
    void CmdMpegSetFade();           // 0xA3
    void CmdMpegSetVideoEffects();   // 0xA4
    void CmdMpegSetLSI();            // 0xAF

    void CmdAuthenticateDevice();    // 0xE0
    void CmdIsDeviceAuthenticated(); // 0xE1
    void CmdGetMpegROM();            // 0xE2
};

} // namespace satemu::cdblock
