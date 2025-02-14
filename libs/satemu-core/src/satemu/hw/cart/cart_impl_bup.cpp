#include <satemu/hw/cart/cart_impl_bup.hpp>

#include <satemu/util/size_ops.hpp>

#include <utility>

namespace satemu::cart {

BackupMemoryCartridge::BackupMemoryCartridge(Size size, const std::filesystem::path &path, std::error_code &error)
    : BaseCartridge(0x21u + static_cast<uint8>(size)) {
    static constexpr size_t kSizes[] = {512_KiB, 1_MiB, 2_MiB, 4_MiB};

    auto index = std::min<uint8>(static_cast<uint8>(size), std::size(kSizes));
    m_backupRAM.LoadFrom(path, kSizes[index], error);
}

} // namespace satemu::cart
