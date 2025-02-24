#include <satemu/hw/sh2/sh2_bus.hpp>

#include <satemu/hw/sh2/sh2.hpp>

#include <fstream>

namespace satemu::sh2 {

template <mem_primitive T, uint32 mask>
static T GenericRead(uint32 address, void *ctx) {
    return util::ReadBE<T>(static_cast<uint8 *>(ctx) + (address & mask));
}

template <mem_primitive T, uint32 mask>
static void GenericWrite(uint32 address, T value, void *ctx) {
    util::WriteBE<T>(static_cast<uint8 *>(ctx) + (address & mask), value);
}

SH2Bus::SH2Bus(SH2 &masterSH2, SH2 &slaveSH2, scu::SCU &scu)
    : m_masterSH2(masterSH2)
    , m_slaveSH2(slaveSH2)
    , m_SCU(scu) {

    static constexpr std::size_t kInternalBackupRAMSize = 32_KiB; // HACK: should be in its own component
    // TODO: configurable path and mode
    std::error_code error{};
    m_internalBackupRAM.LoadFrom("bup-int.bin", kInternalBackupRAMSize, error);
    // TODO: handle error

    UnmapMemory(0x000'0000, 0x7FF'FFFF);

    MapMemory(0x000'0000, 0x00F'FFFF,
              {
                  .ctx = IPL.data(),
                  .read8 = GenericRead<uint8, 0x7FFFF>,
                  .read16 = GenericRead<uint16, 0x7FFFF>,
                  .read32 = GenericRead<uint32, 0x7FFFF>,
                  .write8 = GenericWrite<uint8, 0x7FFFF>,
                  .write16 = GenericWrite<uint16, 0x7FFFF>,
                  .write32 = GenericWrite<uint32, 0x7FFFF>,
              });

    // SMPC maps itself at 0x010'0000..0x017'FFFF

    MapMemory(0x018'0000, 0x01F'FFFF,
              {
                  .ctx = &m_internalBackupRAM,
                  .read8 = [](uint32 address, void *ctx) -> uint8 {
                      return static_cast<bup::BackupMemory *>(ctx)->ReadByte(address);
                  },
                  .read16 = [](uint32 address, void *ctx) -> uint16 {
                      return static_cast<bup::BackupMemory *>(ctx)->ReadWord(address);
                  },
                  .read32 = [](uint32 address, void *ctx) -> uint32 {
                      return static_cast<bup::BackupMemory *>(ctx)->ReadLong(address);
                  },
                  .write8 = [](uint32 address, uint8 value,
                               void *ctx) { static_cast<bup::BackupMemory *>(ctx)->WriteByte(address, value); },
                  .write16 = [](uint32 address, uint16 value,
                                void *ctx) { static_cast<bup::BackupMemory *>(ctx)->WriteWord(address, value); },
                  .write32 = [](uint32 address, uint32 value,
                                void *ctx) { static_cast<bup::BackupMemory *>(ctx)->WriteLong(address, value); },
              });

    MapMemory(0x020'0000, 0x02F'FFFF,
              {
                  .ctx = WRAMLow.data(),
                  .read8 = GenericRead<uint8, 0xFFFFF>,
                  .read16 = GenericRead<uint16, 0xFFFFF>,
                  .read32 = GenericRead<uint32, 0xFFFFF>,
                  .write8 = GenericWrite<uint8, 0xFFFFF>,
                  .write16 = GenericWrite<uint16, 0xFFFFF>,
                  .write32 = GenericWrite<uint32, 0xFFFFF>,
              });

    MapMemory(
        0x100'0000, 0x17F'FFFF,
        {
            .ctx = this,
            .write16 = [](uint32 address, uint16 value, void *ctx) { static_cast<SH2Bus *>(ctx)->WriteMINIT(value); },
        });

    MapMemory(
        0x180'0000, 0x1FF'FFFF,
        {
            .ctx = this,
            .write16 = [](uint32 address, uint16 value, void *ctx) { static_cast<SH2Bus *>(ctx)->WriteSINIT(value); },
        });

    // SCU, VDP, SCSP, CD block and cartridge slot map themselves at 0x200'0000..0x5FF'FFFF

    MapMemory(0x600'0000, 0x7FF'FFFF,
              {
                  .ctx = WRAMHigh.data(),
                  .read8 = GenericRead<uint8, 0xFFFFF>,
                  .read16 = GenericRead<uint16, 0xFFFFF>,
                  .read32 = GenericRead<uint32, 0xFFFFF>,
                  .write8 = GenericWrite<uint8, 0xFFFFF>,
                  .write16 = GenericWrite<uint16, 0xFFFFF>,
                  .write32 = GenericWrite<uint32, 0xFFFFF>,
              });

    IPL.fill(0);
    Reset(true);
}

void SH2Bus::Reset(bool hard) {
    WRAMLow.fill(0);
    WRAMHigh.fill(0);
}

void SH2Bus::LoadIPL(std::span<uint8, kIPLSize> ipl) {
    std::copy(ipl.begin(), ipl.end(), IPL.begin());
}

void SH2Bus::DumpWRAMLow(std::ostream &out) const {
    out.write((const char *)WRAMLow.data(), WRAMLow.size());
}

void SH2Bus::DumpWRAMHigh(std::ostream &out) const {
    out.write((const char *)WRAMHigh.data(), WRAMHigh.size());
}

void SH2Bus::AcknowledgeExternalInterrupt() {
    m_SCU.AcknowledgeExternalInterrupt();
}

void SH2Bus::WriteMINIT(uint16 value) {
    m_slaveSH2.TriggerFRTInputCapture();
}

void SH2Bus::WriteSINIT(uint16 value) {
    m_masterSH2.TriggerFRTInputCapture();
}

void SH2Bus::MapMemory(uint32 start, uint32 end, MemoryPage entry) {
    const uint32 startIndex = start >> kPageGranularityBits;
    const uint32 endIndex = end >> kPageGranularityBits;
    for (uint32 i = startIndex; i <= endIndex; i++) {
        m_pages[i] = entry;
    }
}

void SH2Bus::UnmapMemory(uint32 start, uint32 end) {
    MapMemory(start, end, {});
}

} // namespace satemu::sh2
