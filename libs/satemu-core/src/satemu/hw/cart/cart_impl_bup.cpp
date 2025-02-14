#include <satemu/hw/cart/cart_impl_bup.hpp>

#include <satemu/util/size_ops.hpp>

#include <utility>

namespace satemu::cart {

static constexpr size_t kSizes[] = {512_KiB, 1_MiB, 2_MiB, 4_MiB};
static constexpr uint8 kIDs[] = {0x21, 0x22, 0x23, 0x24};

static constexpr uint8 GetIndex(BackupMemoryCartridge::Size size) {
    return std::min<uint8>(static_cast<uint8>(size), std::size(kSizes));
}

BackupMemoryCartridge::BackupMemoryCartridge(Size size, const std::filesystem::path &path, std::error_code &error)
    : BaseCartridge(kIDs[GetIndex(size)]) {
    m_backupRAM.LoadFrom(path, kSizes[GetIndex(size)], error);
}

} // namespace satemu::cart
