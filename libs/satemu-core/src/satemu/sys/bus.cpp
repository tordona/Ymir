#include <satemu/sys/bus.hpp>

namespace satemu::sys {

void Bus::MapMemory(uint32 start, uint32 end, MemoryPage entry) {
    const uint32 startIndex = start >> kPageGranularityBits;
    const uint32 endIndex = end >> kPageGranularityBits;
    for (uint32 i = startIndex; i <= endIndex; i++) {
        m_pages[i] = entry;
    }
}

void Bus::UnmapMemory(uint32 start, uint32 end) {
    MapMemory(start, end, {});
}

} // namespace satemu::sys
