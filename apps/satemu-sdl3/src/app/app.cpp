#include "app.hpp"

#include "app_impl.hpp"

namespace app {

App::App()
    : m_impl(std::make_unique<Impl>()) {}

App::~App() = default;

int App::Run(const CommandLineOptions &options) {
    return m_impl->Run(options);
}

} // namespace app
