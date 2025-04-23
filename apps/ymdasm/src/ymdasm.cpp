#include <fmt/format.h>

int main(int argc, char *argv[]) {
    fmt::println("ymdasm " Ymir_VERSION);

    // TODO: architecture
    // - command processor (doubles as disassembly engine)
    //   - interface
    //   - concrete instances for:
    //     - base processor (default instance)
    //     - processors for each ISA
    // - command line parser
    //   - reads argc and argv
    //   - parses the command line syntax described in README
    //   - calls command processor methods with structured data
    //   - also processes the ISA switch commands
    // - global options
    //   - shared with all command processors
}
