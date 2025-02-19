#pragma once

#include "app.hpp"

#include <satemu/sys/saturn.hpp>

namespace app {

class App::Impl {
public:
    int Run(const CommandLineOptions &options);

private:
    satemu::Saturn m_saturn;
    CommandLineOptions m_options;

    void RunEmulator();

    // -----------------------------------------------------------------------------------------------------------------
    // Debugger

    struct AppTracer final : public satemu::debug::ITracer {
        AppTracer(Impl &app);

    private:
        Impl &m_app;
    };
};

} // namespace app
