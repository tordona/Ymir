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

    // -----------------------------------------------------------------------------------------------------------------
    // Debugger

    void TestDebug();

    struct AppProbe final : public satemu::debug::IProbe {
        AppProbe(Impl &app);

        void test() final;

    private:
        Impl &m_app;
    };
};

} // namespace app
