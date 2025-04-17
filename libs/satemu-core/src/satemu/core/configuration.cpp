#include <satemu/core/configuration.hpp>

namespace satemu::core {

void Configuration::NotifyObservers() {
    system.preferredRegionOrder.Notify();

    video.threadedVDP.Notify();

    audio.interpolation.Notify();
    audio.threadedSCSP.Notify();
}

} // namespace satemu::core
