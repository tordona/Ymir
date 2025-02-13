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

SH2Bus::SH2Bus(SH2 &masterSH2, SH2 &slaveSH2, scu::SCU &scu, smpc::SMPC &smpc)
    : m_masterSH2(masterSH2)
    , m_slaveSH2(slaveSH2)
    , m_SCU(scu)
    , m_SMPC(smpc) {

    // HACK: should be in its own class, shared with external backup RAM cartridges
    static constexpr std::string_view kHeader = "BackUpRam Format";
    std::filesystem::path bupRAMPath = "bup-int.bin";
    if (!std::filesystem::is_regular_file(bupRAMPath) ||
        std::filesystem::file_size(bupRAMPath) < kInternalBackupRAMSize) {
        std::ofstream out{bupRAMPath, std::ios::binary};

        // Write header at the beginning
        for (int i = 0; i < 4; i++) {
            for (char ch : kHeader) {
                out.put(0xFF);
                out.put(ch);
            }
        }

        // Clear the rest of the file
        out.seekp(0, std::ios::end);
        if (out.tellp() & 1) {
            out.put(0);
        }
        for (size_t i = out.tellp(); i < kInternalBackupRAMSize; i += 2) {
            out.put(0xFF);
            out.put(0);
        }
    }
    std::error_code err{};
    internalBackupRAM = mio::make_mmap_sink(bupRAMPath.string(), err);
    // TODO: handle error

    MapMemory(
        0x000'0000, 0x7FF'FFFF,
        {
            .read8 = [](uint32 address, void *ctx) -> uint8 {
                rootLog.debug("Unhandled 8-bit read from {:07X}", address);
                return 0;
            },
            .read16 = [](uint32 address, void *ctx) -> uint16 {
                rootLog.debug("Unhandled 16-bit read from {:07X}", address);
                return 0;
            },
            .read32 = [](uint32 address, void *ctx) -> uint32 {
                rootLog.debug("Unhandled 32-bit read from {:07X}", address);
                return 0;
            },
            .write8 = [](uint32 address, uint8 value,
                         void *ctx) { rootLog.debug("Unhandled 8-bit write to {:07X} = {:02X}", address, value); },
            .write16 = [](uint32 address, uint16 value,
                          void *ctx) { rootLog.debug("Unhandled 16-bit write to {:07X} = {:04X}", address, value); },
            .write32 = [](uint32 address, uint32 value,
                          void *ctx) { rootLog.debug("Unhandled 32-bit write to {:07X} = {:07X}", address, value); },
        });

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

    MapMemory(0x010'0000, 0x017'FFFF,
              {
                  .ctx = &m_SMPC,
                  .read8 = [](uint32 address, void *ctx) -> uint8 {
                      return static_cast<smpc::SMPC *>(ctx)->Read((address & 0x7F) | 1);
                  },
                  .read16 = [](uint32 address, void *ctx) -> uint16 {
                      return static_cast<smpc::SMPC *>(ctx)->Read((address & 0x7F) | 1);
                  },
                  .read32 = [](uint32 address, void *ctx) -> uint32 {
                      return static_cast<smpc::SMPC *>(ctx)->Read((address & 0x7F) | 1);
                  },
                  .write8 = [](uint32 address, uint8 value,
                               void *ctx) { static_cast<smpc::SMPC *>(ctx)->Write((address & 0x7F) | 1, value); },
                  .write16 = [](uint32 address, uint16 value,
                                void *ctx) { static_cast<smpc::SMPC *>(ctx)->Write((address & 0x7F) | 1, value); },
                  .write32 = [](uint32 address, uint32 value,
                                void *ctx) { static_cast<smpc::SMPC *>(ctx)->Write((address & 0x7F) | 1, value); },
              });

    MapMemory(0x018'0000, 0x01F'FFFF,
              {
                  .ctx = internalBackupRAM.data(),
                  .read8 = GenericRead<uint8, kInternalBackupRAMSize - 1>,
                  .read16 = GenericRead<uint16, kInternalBackupRAMSize - 1>,
                  .read32 = GenericRead<uint32, kInternalBackupRAMSize - 1>,
                  .write8 = GenericWrite<uint8, kInternalBackupRAMSize - 1>,
                  .write16 = GenericWrite<uint16, kInternalBackupRAMSize - 1>,
                  .write32 = GenericWrite<uint32, kInternalBackupRAMSize - 1>,
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

    // SCU maps itself in this region
    /*MapMemory(0x200'0000, 0x5FF'FFFF,
              {
                  .ctx = &m_SCU,
                  .read8 = [](uint32 address, void *ctx) -> uint8 {
                      return static_cast<scu::SCU *>(ctx)->Read<uint8>(address);
                  },
                  .read16 = [](uint32 address, void *ctx) -> uint16 {
                      return static_cast<scu::SCU *>(ctx)->Read<uint16>(address);
                  },
                  .read32 = [](uint32 address, void *ctx) -> uint32 {
                      return static_cast<scu::SCU *>(ctx)->Read<uint32>(address);
                  },
                  .write8 = [](uint32 address, uint8 value,
                               void *ctx) { static_cast<scu::SCU *>(ctx)->Write<uint8>(address, value); },
                  .write16 = [](uint32 address, uint16 value,
                                void *ctx) { static_cast<scu::SCU *>(ctx)->Write<uint16>(address, value); },
                  .write32 = [](uint32 address, uint32 value,
                                void *ctx) { static_cast<scu::SCU *>(ctx)->Write<uint32>(address, value); },
              });*/

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

void SH2Bus::MapMemory(uint32 start, uint32 end, MemoryRegionEntry entry) {
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
