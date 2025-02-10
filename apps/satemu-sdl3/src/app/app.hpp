#pragma once

#include "cmdline_opts.hpp"

#include <memory>

namespace app {

class App {
public:
    App();
    ~App();

    int Run(const CommandLineOptions &options);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace app
