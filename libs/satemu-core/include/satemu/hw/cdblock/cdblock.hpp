#pragma once

#include "cdblock_defs.hpp"

#include "cdblock_buffer.hpp"
#include "cdblock_filter.hpp"

#include <satemu/core/scheduler.hpp>
#include <satemu/sys/system.hpp>

#include <satemu/hw/hw_defs.hpp>

#include <satemu/media/disc.hpp>
#include <satemu/media/filesystem.hpp>

#include <satemu/util/debug_print.hpp>

#include <array>
#include <deque>

// -----------------------------------------------------------------------------
// Forward declarations

namespace satemu {

struct Saturn;

} // namespace satemu

namespace satemu::sh2 {

class SH2Bus;

} // namespace satemu::sh2

namespace satemu::scu {

class SCU;

} // namespace satemu::scu

namespace satemu::scsp {

class SCSP;

} // namespace satemu::scsp

// -----------------------------------------------------------------------------

namespace satemu::cdblock {

class CDBlock {
    static constexpr dbg::Category rootLog{"CDBlock"};
    static constexpr dbg::Category regsLog{rootLog, "Regs"};
    static constexpr dbg::Category playInitLog{rootLog, "PlayInit"};
    static constexpr dbg::Category playLog{rootLog, "Play"};
    static constexpr dbg::Category xferLog{rootLog, "Transfer"};
    static constexpr dbg::Category partLog{rootLog, "PartMgr"};

public:
    CDBlock(sys::System &system, core::Scheduler &scheduler, scu::SCU &scu, scsp::SCSP &scsp);

    void Reset(bool hard);

    void LoadDisc(media::Disc &&disc);
    void EjectDisc();
    void OpenTray();
    void CloseTray();
    bool IsTrayOpen() const;

    const media::Disc &GetDisc() const {
        return m_disc;
    }

private:
    sys::System &m_system;
    scu::SCU &m_SCU;
    scsp::SCSP &m_SCSP;

    core::Scheduler &m_scheduler;
    core::EventID m_driveStateUpdateEvent;
    core::EventID m_commandExecEvent;

    template <bool debug>
    static void OnDriveStateUpdateEvent(core::EventContext &eventContext, void *userContext, uint64 cyclesLate);

    template <bool debug>
    static void OnCommandExecEvent(core::EventContext &eventContext, void *userContext, uint64 cyclesLate);

    friend struct satemu::Saturn;
    void UpdateClockRatios();

    alignas(uint64) std::array<uint16, 4> m_CR;

    // TODO: use a device instead, to support reading from real drives as well as disc images
    media::Disc m_disc;
    media::fs::Filesystem m_fs;

    // -------------------------------------------------------------------------
    // Memory accessors (SCU-facing bus)
    // 16-bit reads, 8- or 16-bit writes

    friend struct ::satemu::Saturn;
    void MapMemory(sh2::SH2Bus &bus);

    // TODO: handle 8-bit and 32-bit accesses properly

    template <mem_primitive T>
    T ReadReg(uint32 address);

    template <mem_primitive T>
    void WriteReg(uint32 address, T value);

    // -------------------------------------------------------------------------
    // Disc/drive state

    struct Status {
        // Status code, one of kStatusCode* constants and kStatusFlag* flags, or kStatusReject.
        // kStatusFlagPeriodic and kStatusFlagWait are mutually exclusive.
        uint8 statusCode;

        uint32 frameAddress; // current frame address
        uint8 flags;         // bit 7: 1=reading CD-ROM data; 0=reading CD-DA, seeking, scanning, etc.
        uint8 repeatCount;   // bits 3-0: repeat count
        uint8 controlADR;    // control/ADR bits of the current track
        uint8 track;         // current track
        uint8 index;         // current index
    } m_status;

    bool m_readyForPeriodicReports; // HACK to avoid overwriting the initial state during the boot sequence

    uint32 m_currDriveCycles;   // current cycle count for drive state processing
    uint32 m_targetDriveCycles; // number of cycles until the next drive state update

    // PlayDisc parameters
    uint32 m_playStartParam; // starting frame address or track/index
    uint32 m_playEndParam;   // ending frame address or track/index
    uint8 m_playRepeatParam; // playback repeat count parameter

    // Playback status/parameters
    uint32 m_playStartPos;  // starting frame address for playback
    uint32 m_playEndPos;    // ending frame address for playback
    uint8 m_playMaxRepeat;  // max repeat count (0=no repeat, 1..14=N repeats, 15=infinite repeats)
    bool m_playFile;        // is playback reading a file?
    bool m_bufferFullPause; // paused because of running out of buffers?

    uint8 m_readSpeed;

    // CD authentication status:
    //   0: no CD/not authenticated
    //   1: audio CD
    //   2: non-Saturn CD
    //   3: non-original Saturn CD
    //   4: original Saturn CD
    uint8 m_discAuthStatus;

    // MPEG authentication status:
    //   0: no MPEG card/not authenticated
    //   2: MPEG card present
    uint8 m_mpegAuthStatus;

    bool SetupGenericPlayback(uint32 startParam, uint32 endParam, uint16 repeatParam);
    bool SetupFilePlayback(uint32 fileID, uint32 offset, uint8 filterNumber);

    template <bool debug>
    void ProcessDriveState();

    void ProcessDriveStatePlay();

    // -------------------------------------------------------------------------
    // Interrupts

    uint16 m_HIRQ;
    uint16 m_HIRQMASK;

    void SetInterrupt(uint16 bits);
    void UpdateInterrupts();

    // -------------------------------------------------------------------------
    // Status reports

    // Updates CR1-4 with the current CD status
    void ReportCDStatus();

    // Updates CR1-4 with the current CD status, overriding the status code
    void ReportCDStatus(uint8 statusCode);

    // -------------------------------------------------------------------------
    // Data transfers

    enum class TransferType { None, TOC, GetSector, GetThenDeleteSector, FileInfo, Subcode };

    // General transfer parameters
    TransferType m_xferType; // Type of transfer in progress
    uint32 m_xferPos;        // Current transfer position in words
    uint32 m_xferLength;     // Total number of words to be transferred
    uint32 m_xferCount;      // Number of words transferred in the last transfer

    // Parameters for sector transfers
    uint32 m_xferSectorPos; // Current transfer sector position
    uint32 m_xferSectorEnd; // Last sector to transfer
    uint8 m_xferPartition;  // From which partition to read
    // uint8 m_xferFilter;      // To which filter to write

    // Parameters for file info transfers
    uint32 m_xferCurrFileID; // Current file ID to read

    // Buffer for subcode transfers
    std::array<uint8, 24> m_xferSubcodeBuffer; // Buffer for subcode transfer data
    uint32 m_xferSubcodeFrameAddress;          // Last subcode R-W frame address
    uint32 m_xferSubcodeGroup;                 // Last subcode R-W group

    // Debugging data
    uint32 m_xferExtraCount; // Number of additional/unexpected reads/writes

    void SetupTOCTransfer();
    void SetupGetSectorTransfer(uint16 sectorPos, uint16 sectorCount, uint8 partitionNumber, bool del);
    uint32 SetupFileInfoTransfer(uint32 fileID);
    bool SetupSubcodeTransfer(uint8 type);
    void EndTransfer();

    uint16 DoReadTransfer();
    void DoWriteTransfer(uint16 value);

    void AdvanceTransfer();

    // -------------------------------------------------------------------------
    // Buffers, partitions and filters
    //
    // The low-level storage unit is the buffer, which stores one sector of 2352 bytes worth of data.
    // The CD block contains 202 buffers, but only 200 are accessible externally.
    //
    // A buffer partition is a logical group of buffers containing a continuous section of data. The partitions are only
    // limited by the total buffer capacity of 200 blocks and can store buffers in any order, much like virtual memory
    // allocations backed by physical memory in systems with MMUs.
    //
    // All streamed data passes through a configurable set of 24 filters that conditionally route data to one of two
    // outputs: "true" or "false", or, more appropriately, "accept" and "reject". There are also 24 buffer partitions
    // used as a staging area for transfers. Every filter and buffer partition has an input and output connector. By
    // default, all filter inputs and buffer partition outputs are disconnected, and filter output connectors are routed
    // to the buffer partition inputs of the same index.
    //
    // The CD block can receive data from these devices that expose an output connector:
    // - The CD drive
    // - The host SH-2 CPU (via writes to the data transfer register on port 0x98000)
    // - The MPEG decoder, which contains the MPEG frame buffer and MPEG sector buffer
    //
    // Data can be streamed out to these devices that expose an input connector:
    // - The host SH-2 CPU (via reads from the data transfer register on port 0x98000)
    // - The MPEG decoder:
    //   - Audio output
    //   - Video output
    //   - Frame buffer (directly connected to the VDP2's EXBG)
    //   - Sector buffer
    //
    // Connections from and to devices are configured by SetCDDeviceConnection, MpegSetConnection, and several transfer
    // commands which make the data accessible by the SH-2 via port 0x98000.
    //
    // Connections are constrained to the following rules:
    // - Output connectors from devices can only be assigned to filter input connectors.
    // - The "true" output connector of a filter can only be routed to the input connector of a buffer partition.
    //   A buffer partition may receive any number of inputs. Data received from multiple inputs will be concatenated.
    // - The "false" output connector of a filter can only be assigned to a filter's input connector. The filter may
    //   output data to itself or another filter.
    // - The buffer partition output connector can be assigned to a device input connector or a filter's input connector
    //   through the copy/move commands.
    // - Only one connection can be made to filter input connectors. Attempting to connect another output to a filter
    //   input will sever the existing connection.
    //
    // Disconnected filter output connectors will result in dropping the data.

    class PartitionManager {
    public:
        PartitionManager();

        void Reset();

        uint8 GetBufferCount(uint8 partitionIndex) const;
        uint32 GetFreeBufferCount() const;

        void InsertHead(uint8 partitionIndex, Buffer &buffer);
        Buffer *GetTail(uint8 partitionIndex, uint8 offset);
        bool RemoveTail(uint8 partitionIndex, uint8 offset);

        uint32 DeleteSectors(uint8 partitionIndex, uint16 sectorPos, uint16 sectorCount);

        void Clear(uint8 partitionIndex);

        uint32 CalculateSize(uint8 partitionIndex, uint32 start, uint32 end) const;

    private:
        std::array<std::deque<Buffer>, kNumPartitions> m_partitions;

        uint32 m_freeBuffers;
    };

    PartitionManager m_partitionManager;
    std::array<Filter, kNumFilters> m_filters;

    Buffer m_scratchBuffer;

    uint8 m_cdDeviceConnection;
    uint8 m_lastCDWritePartition;

    uint32 m_calculatedPartitionSize;

    uint32 m_getSectorLength;
    uint32 m_putSectorLength;

    bool ConnectCDDevice(uint8 filterNumber);

    void DisconnectFilterInput(uint8 filterNumber);

    // -------------------------------------------------------------------------
    // Commands

    bool m_processingCommand; // true if a command is in progress

    void SetupCommand();

    template <bool debug>
    void ProcessCommand();

    // General CD block operations
    void CmdGetStatus();          // 0x00
    void CmdGetHardwareInfo();    // 0x01
    void CmdGetTOC();             // 0x02
    void CmdGetSessionInfo();     // 0x03
    void CmdInitializeCDSystem(); // 0x04
    void CmdOpenTray();           // 0x05
    void CmdEndDataTransfer();    // 0x06

    // Basic CD playback operations
    void CmdPlayDisc(); // 0x10
    void CmdSeekDisc(); // 0x11
    void CmdScanDisc(); // 0x12

    // Subcode retrieval
    void CmdGetSubcodeQ_RW(); // 0x20

    // CD-ROM device connection
    void CmdSetCDDeviceConnection(); // 0x30
    void CmdGetCDDeviceConnection(); // 0x31
    void CmdGetLastBufferDest();     // 0x32

    // Filters
    void CmdSetFilterRange();               // 0x40
    void CmdGetFilterRange();               // 0x41
    void CmdSetFilterSubheaderConditions(); // 0x42
    void CmdGetFilterSubheaderConditions(); // 0x43
    void CmdSetFilterMode();                // 0x44
    void CmdGetFilterMode();                // 0x45
    void CmdSetFilterConnection();          // 0x46
    void CmdGetFilterConnection();          // 0x47
    void CmdResetSelector();                // 0x48

    // Buffers and buffer partitions
    void CmdGetBufferSize();       // 0x50
    void CmdGetSectorNumber();     // 0x51
    void CmdCalculateActualSize(); // 0x52
    void CmdGetActualSize();       // 0x53
    void CmdGetSectorInfo();       // 0x54
    void CmdExecuteFADSearch();    // 0x55
    void CmdGetFADSearchResults(); // 0x56

    // Buffer input and output
    void CmdSetSectorLength();         // 0x60
    void CmdGetSectorData();           // 0x61
    void CmdDeleteSectorData();        // 0x62
    void CmdGetThenDeleteSectorData(); // 0x63
    void CmdPutSectorData();           // 0x64
    void CmdCopySectorData();          // 0x65
    void CmdMoveSectorData();          // 0x66
    void CmdGetCopyError();            // 0x67

    // File system operations
    void CmdChangeDirectory();    // 0x70
    void CmdReadDirectory();      // 0x71
    void CmdGetFileSystemScope(); // 0x72
    void CmdGetFileInfo();        // 0x73
    void CmdReadFile();           // 0x74
    void CmdAbortFile();          // 0x75

    // MPEG decoder
    void CmdMpegGetStatus();         // 0x90
    void CmdMpegGetInterrupt();      // 0x91
    void CmdMpegSetInterruptMask();  // 0x92
    void CmdMpegInit();              // 0x93
    void CmdMpegSetMode();           // 0x94
    void CmdMpegPlay();              // 0x95
    void CmdMpegSetDecodingMethod(); // 0x96

    // MPEG stream
    void CmdMpegSetConnection(); // 0x9A
    void CmdMpegGetConnection(); // 0x9B
    void CmdMpegSetStream();     // 0x9D
    void CmdMpegGetStream();     // 0x9E

    // MPEG display screen
    void CmdMpegDisplay();         // 0xA0
    void CmdMpegSetWindow();       // 0xA1
    void CmdMpegSetBorderColor();  // 0xA2
    void CmdMpegSetFade();         // 0xA3
    void CmdMpegSetVideoEffects(); // 0xA4
    void CmdMpegSetLSI();          // 0xAF

    void CmdAuthenticateDevice();    // 0xE0
    void CmdIsDeviceAuthenticated(); // 0xE1
    void CmdGetMpegROM();            // 0xE2
};

} // namespace satemu::cdblock
