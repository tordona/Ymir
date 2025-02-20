#include <satemu/debug/debug_tracer.hpp>

#include <satemu/debug/debug_tracer_sh2.hpp>

namespace satemu::debug {

TracerContext::TracerContext()
    : m_masterSH2(std::make_unique<SH2>(true))
    , m_slaveSH2(std::make_unique<SH2>(false)) {}

TracerContext::~TracerContext() = default;

void TracerContext::UpdateContexts() {
    if (m_tracer != nullptr) {
        m_masterSH2->tracer = &m_tracer->GetMasterSH2Tracer();
        m_slaveSH2->tracer = &m_tracer->GetSlaveSH2Tracer();
    } else {
        m_masterSH2->tracer = nullptr;
        m_slaveSH2->tracer = nullptr;
    }
}

} // namespace satemu::debug
