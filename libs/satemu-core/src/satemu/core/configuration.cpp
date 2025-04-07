#include <satemu/core/configuration.hpp>

namespace satemu::core {

void Configuration::NotifyObservers() {
    system.preferredRegionOrder.Notify();

    video.threadedVDP1.Notify();
    video.threadedVDP2.Notify();

    audio.interpolation.Notify();
    audio.threadedSCSP.Notify();
}

} // namespace satemu::core
