#include <satemu/sys/memory.hpp>

#include <satemu/hw/hw_defs.hpp>

#include <satemu/util/data_ops.hpp>

namespace satemu::sys {

template <mem_primitive T, uint32 mask>
static T GenericRead(uint32 address, void *ctx) {
    return util::ReadBE<T>(static_cast<uint8 *>(ctx) + (address & mask));
}

template <mem_primitive T, uint32 mask>
static void GenericWrite(uint32 address, T value, void *ctx) {
    util::WriteBE<T>(static_cast<uint8 *>(ctx) + (address & mask), value);
}

SystemMemory::SystemMemory() {
    // TODO: configurable path and mode
    std::error_code error{};
    internalBackupRAM.LoadFrom("bup-int.bin", kInternalBackupRAMSize, error);
    // TODO: handle error

    IPL.fill(0);
    Reset(true);
}

void SystemMemory::Reset(bool hard) {
    if (hard) {
        WRAMLow.fill(0);
        WRAMHigh.fill(0);
    }
}

void SystemMemory::MapMemory(Bus &bus) {
    bus.MapMemory(0x000'0000, 0x00F'FFFF,
                  {
                      .ctx = IPL.data(),
                      .read8 = GenericRead<uint8, 0x7FFFF>,
                      .read16 = GenericRead<uint16, 0x7FFFF>,
                      .read32 = GenericRead<uint32, 0x7FFFF>,
                      .write8 = GenericWrite<uint8, 0x7FFFF>,
                      .write16 = GenericWrite<uint16, 0x7FFFF>,
                      .write32 = GenericWrite<uint32, 0x7FFFF>,
                  });

    bus.MapMemory(0x018'0000, 0x01F'FFFF,
                  {
                      .ctx = &internalBackupRAM,
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

    bus.MapMemory(0x020'0000, 0x02F'FFFF,
                  {
                      .ctx = WRAMLow.data(),
                      .read8 = GenericRead<uint8, 0xFFFFF>,
                      .read16 = GenericRead<uint16, 0xFFFFF>,
                      .read32 = GenericRead<uint32, 0xFFFFF>,
                      .write8 = GenericWrite<uint8, 0xFFFFF>,
                      .write16 = GenericWrite<uint16, 0xFFFFF>,
                      .write32 = GenericWrite<uint32, 0xFFFFF>,
                  });

    bus.MapMemory(0x600'0000, 0x7FF'FFFF,
                  {
                      .ctx = WRAMHigh.data(),
                      .read8 = GenericRead<uint8, 0xFFFFF>,
                      .read16 = GenericRead<uint16, 0xFFFFF>,
                      .read32 = GenericRead<uint32, 0xFFFFF>,
                      .write8 = GenericWrite<uint8, 0xFFFFF>,
                      .write16 = GenericWrite<uint16, 0xFFFFF>,
                      .write32 = GenericWrite<uint32, 0xFFFFF>,
                  });
}

void SystemMemory::LoadIPL(std::span<uint8, kIPLSize> ipl) {
    std::copy(ipl.begin(), ipl.end(), IPL.begin());
}

void SystemMemory::DumpWRAMLow(std::ostream &out) const {
    out.write((const char *)WRAMLow.data(), WRAMLow.size());
}

void SystemMemory::DumpWRAMHigh(std::ostream &out) const {
    out.write((const char *)WRAMHigh.data(), WRAMHigh.size());
}

} // namespace satemu::sys
