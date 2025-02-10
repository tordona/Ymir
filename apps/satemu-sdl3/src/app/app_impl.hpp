#pragma once

#include "app.hpp"

#include <satemu/satemu.hpp>

namespace app {

class App::Impl {
public:
    int Run(const CommandLineOptions &options);

private:
    satemu::Saturn m_saturn;

    void RunEmulator();
};

} // namespace app
