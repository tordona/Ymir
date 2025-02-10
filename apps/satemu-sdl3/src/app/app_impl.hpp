#pragma once

#include "app.hpp"

#include <satemu/sys/saturn.hpp>

namespace app {

class App::Impl {
public:
    int Run(const CommandLineOptions &options);

private:
    satemu::Saturn m_saturn;

    void RunEmulator();
};

} // namespace app
