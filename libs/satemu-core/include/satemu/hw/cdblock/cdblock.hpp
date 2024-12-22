#pragma once

#include "cdblock_defs.hpp"

#include <satemu/hw/hw_defs.hpp>

#include <satemu/media/disc.hpp>
#include <satemu/media/filesystem.hpp>

#include <fmt/format.h>

#include <array>
#include <cassert>

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

    void LoadDisc(media::Disc &&disc);
    void EjectDisc();
    void OpenTray();
    void CloseTray();

    void Advance(uint64 cycles);

    // TODO: handle 8-bit and 32-bit accesses properly

    template <mem_primitive T>
    T ReadReg(uint32 address) {
        /*T value = ReadRegImpl<T>(address);
        fmt::println("{}-bit CD Block register read from {:02X} = {:X}", sizeof(T) * 8, address, value);
        return value;
    }

    template <mem_primitive T>
    T ReadRegImpl(uint32 address) {*/
        switch (address) {
        case 0x00: return DoReadTransfer();
        case 0x02: return DoReadTransfer();
        case 0x08: return m_HIRQ;
        case 0x0C: return m_HIRQMASK;
        case 0x18: return m_CR[0];
        case 0x1C: return m_CR[1];
        case 0x20: return m_CR[2];
        case 0x24:
            m_processingCommand = false;
            m_readyForPeriodicReports = true;
            return m_CR[3];
        default: fmt::println("unhandled {}-bit CD Block register read from {:02X}", sizeof(T) * 8, address); return 0;
        }
    }

    template <mem_primitive T>
    void WriteReg(uint32 address, T value) {
        // fmt::println("{}-bit CD Block register write to {:02X} = {:X}", sizeof(T) * 8, address, value);
        switch (address) {
        case 0x00: DoWriteTransfer(value); break;
        case 0x02: DoWriteTransfer(value); break;
        case 0x08:
            m_HIRQ &= value;
            UpdateInterrupts();
            break;
        case 0x0C:
            m_HIRQMASK = value;
            UpdateInterrupts();
            break;
        case 0x18:
            m_processingCommand = true;
            m_status.statusCode &= ~kStatusFlagPeriodic;
            m_CR[0] = value;
            break;
        case 0x1C: m_CR[1] = value; break;
        case 0x20: m_CR[2] = value; break;
        case 0x24:
            m_CR[3] = value;
            SetupCommand();
            break;

        default:
            fmt::println("unhandled {}-bit CD Block register write to {:02X} = {:X}", sizeof(T) * 8, address, value);
            break;
        }
    }

private:
    scu::SCU &m_scu;

    alignas(uint64) std::array<uint16, 4> m_CR;

    // TODO: use a device instead, to support reading from real drives as well as disc images
    media::Disc m_disc;
    media::fs::Filesystem m_fs;

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
    uint32 m_playStartPos; // starting frame address for playback
    uint32 m_playEndPos;   // ending frame address for playback
    uint8 m_playMaxRepeat; // max repeat count (0=no repeat, 1..14=N repeats, 15=infinite repeats)
    bool m_playFile;       // is playback reading a file?

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

    enum class TransferType { None, TOC, GetSector, GetThenDeleteSector, FileInfo };

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

    void SetupTOCTransfer();
    void SetupGetSectorTransfer(uint16 sectorPos, uint16 sectorCount, uint8 partitionNumber, bool del);
    uint32 SetupFileInfoTransfer(uint32 fileID);
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

    struct Buffer {
        std::array<uint8, 2352> data;
        uint16 size;
        Buffer *prev;
        Buffer *next;
    };

    class BufferManager {
    public:
        BufferManager() {
            Reset();
        }

        void Reset() {
            Buffer *prev = nullptr;
            for (auto &buffer : m_buffers) {
                buffer.data.fill(0);
                buffer.prev = prev;
                buffer.next = nullptr;
                if (prev) {
                    prev->next = &buffer;
                }
                prev = &buffer;
            }

            m_freeListHead = &m_buffers[0];
            m_freeCount = m_buffers.size();
        }

        // Attempts to allocate a buffer.
        // Returns nullptr if there are no free buffers.
        Buffer *Allocate() {
            if (m_freeListHead == nullptr) {
                return nullptr;
            }
            assert(m_freeCount > 0 && m_freeCount <= m_buffers.size());

            // Remove buffer from free list head
            Buffer *buffer = m_freeListHead;
            m_freeListHead = buffer->next;
            m_freeCount--;
            assert((m_freeListHead == nullptr) == (m_freeCount == 0));

            // Detach buffer from linked list
            m_freeListHead->prev = nullptr;
            buffer->next = nullptr;
            assert(buffer->prev == nullptr);

            return buffer;
        }

        void Free(Buffer *buffer) {
            // Detach buffer from existing linked list
            if (buffer->prev) {
                buffer->prev->next = buffer->next;
            }
            if (buffer->next) {
                buffer->next->prev = buffer->prev;
            }

            // Insert it at the head of the free buffer list
            if (m_freeListHead != nullptr) {
                m_freeListHead->prev = buffer;
            }
            buffer->prev = nullptr;
            buffer->next = m_freeListHead;
            m_freeListHead = buffer;

            m_freeCount++;
        }

        // Frees an entire chain of buffers.
        // head must point to the head of the chain.
        void FreeAll(Buffer *head) {
            if (head == nullptr) [[unlikely]] {
                return;
            }
            assert(head->prev == nullptr);

            // Find the tail and relink the free list from it
            Buffer *tail = head;
            m_freeCount++;
            for (; tail->next != nullptr; tail = tail->next) {
                m_freeCount++;
            }

            tail->next = m_freeListHead;
            m_freeListHead->prev = tail;
            m_freeListHead = head;
        }

        // Frees a range of buffers.
        // head and tail must belong to the same chain.
        // null head means "free until the start of the chain".
        // null tail means "free until the end of the chain".
        void FreeRange(Buffer *head, Buffer *tail) {
            // TODO: implement
        }

        uint8 FreeBufferCount() const {
            return m_freeCount;
        }

        constexpr uint8 TotalBufferCount() const {
            return m_buffers.size();
        }

    private:
        std::array<Buffer, 200> m_buffers;
        Buffer *m_freeListHead;
        uint8 m_freeCount;
    };

    class PartitionManager {
    public:
        PartitionManager(BufferManager &bufferManager)
            : m_bufferManager(bufferManager) {
            Reset();
        }

        void Reset() {
            m_partitions.fill({});
        }

        void InsertHead(uint8 partitionIndex, Buffer *buffer) {
            assert(partitionIndex < m_partitions.size());
            assert(buffer != nullptr);

            m_partitions[partitionIndex].InsertHead(buffer);
        }

        Buffer *GetHead(uint8 partitionIndex) {
            assert(partitionIndex < m_partitions.size());
            return m_partitions[partitionIndex].GetHead();
        }

        Buffer *GetTail(uint8 partitionIndex) {
            assert(partitionIndex < m_partitions.size());
            return m_partitions[partitionIndex].GetTail();
        }

        Buffer *RemoveHead(uint8 partitionIndex) {
            assert(partitionIndex < m_partitions.size());

            Buffer *buffer = m_partitions[partitionIndex].RemoveHead();
            m_bufferManager.Free(buffer);
            return buffer;
        }

        Buffer *RemoveTail(uint8 partitionIndex) {
            assert(partitionIndex < m_partitions.size());

            Buffer *buffer = m_partitions[partitionIndex].RemoveTail();
            m_bufferManager.Free(buffer);
            return buffer;
        }

        void ClearAll() {
            for (auto &partition : m_partitions) {
                Buffer *head = partition.Clear();
                m_bufferManager.FreeAll(head);
            }
        }

        void Clear(uint8 partitionIndex) {
            if (partitionIndex < m_partitions.size()) [[likely]] {
                Buffer *head = m_partitions[partitionIndex].Clear();
                m_bufferManager.FreeAll(head);
            }
        }

        uint8 GetBufferCount(uint8 partitionIndex) const {
            assert(partitionIndex < m_partitions.size());
            return m_partitions[partitionIndex].GetBufferCount();
        }

        constexpr uint8 PartitionCount() const {
            return m_partitions.size();
        }

        uint32 CalculateSize(uint8 partitionIndex, uint32 start, uint32 end) const {
            assert(partitionIndex < m_partitions.size());
            return m_partitions[partitionIndex].CalculateSize(start, end);
        }

        // TODO: get buffer at index?
        // TODO: remove buffer range

    private:
        struct Partition {
            // NOTE: must free the buffers manually!
            Buffer *Clear() {
                Buffer *head = m_head;
                m_head = m_tail = nullptr;
                m_bufferCount = 0;
                return head;
            }

            void InsertHead(Buffer *buffer) {
                if (m_head == nullptr) {
                    assert(m_tail == nullptr);
                    assert(m_bufferCount == 0);
                    m_head = m_tail = buffer;
                    m_bufferCount = 1;
                } else {
                    assert(buffer->prev == nullptr);
                    assert(buffer->next == nullptr);
                    buffer->next = m_head;
                    m_head->prev = buffer;
                    m_head = buffer;
                    m_bufferCount++;
                }
            }

            Buffer *GetHead() const {
                return m_head;
            }

            Buffer *GetTail() const {
                return m_tail;
            }

            Buffer *RemoveHead() {
                Buffer *head = m_head;
                if (head == nullptr) [[unlikely]] {
                    return nullptr;
                }
                assert(m_head->prev == nullptr);
                assert(m_bufferCount > 0);
                m_head = m_head->next;
                head->next = nullptr;
                if (m_head != nullptr) {
                    m_head->prev = nullptr;
                } else {
                    assert(head == m_tail);
                    assert(m_bufferCount == 1);
                    m_tail = nullptr;
                }
                m_bufferCount--;

                return head;
            }

            Buffer *RemoveTail() {
                Buffer *tail = m_tail;
                if (tail == nullptr) [[unlikely]] {
                    return nullptr;
                }
                assert(m_tail->next == nullptr);
                assert(m_bufferCount > 0);
                m_tail = m_tail->prev;
                tail->prev = nullptr;
                if (m_tail != nullptr) {
                    m_tail->next = nullptr;
                } else {
                    assert(tail == m_head);
                    assert(m_bufferCount == 1);
                    m_head = nullptr;
                }
                m_bufferCount--;

                return tail;
            }

            uint8 GetBufferCount() const {
                return m_bufferCount;
            }

            uint32 CalculateSize(uint32 start, uint32 end) const {
                uint32 pos = 0;
                Buffer *buf = m_tail;
                while (pos < start) {
                    if (buf == nullptr) {
                        return 0;
                    }
                    buf = buf->prev;
                    pos++;
                }

                uint32 size = 0;
                while (pos <= end) {
                    if (buf == nullptr) {
                        break;
                    }
                    size += buf->size;
                    buf = buf->prev;
                    pos++;
                }
                return size;
            }

        private:
            Buffer *m_head = nullptr;
            Buffer *m_tail = nullptr;
            uint8 m_bufferCount = 0;
        };

        std::array<Partition, 24> m_partitions;
        BufferManager &m_bufferManager;
    };

    BufferManager m_bufferManager;
    PartitionManager m_partitionManager{m_bufferManager};
    std::array<media::Filter, 24> m_filters;

    uint8 m_cdDeviceConnection;
    uint8 m_lastCDWritePartition;

    uint32 m_calculatedPartitionSize;

    uint32 m_getSectorLength;
    uint32 m_putSectorLength;

    bool ConnectCDDevice(uint8 filterNumber);

    void DisconnectFilterInput(uint8 filterNumber);

    // -------------------------------------------------------------------------
    // Commands

    bool m_processingCommand;     // true if a command is in progress
    uint32 m_currCommandCycles;   // current cycle count for commands
    uint32 m_targetCommandCycles; // command is executed when current >= target

    void SetupCommand();

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
