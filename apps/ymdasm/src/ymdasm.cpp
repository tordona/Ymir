#include <fmt/format.h>

int main(int argc, char *argv[]) {
    fmt::println("ymdasm " Ymir_VERSION);

    // TODO: architecture
    // - command processor (doubles as disassembly engine)
    //   - interface
    //   - abstract base class for ISA processors
    //     - concrete classes for each ISA
    //   - concrete class for initial processor (default)
    // - command line parser
    //   - reads argc and argv
    //   - parses the command line syntax described in README
    //   - calls command processor methods with structured data
    //   - also processes the ISA switch commands
    // - global options shared with all command processors
}
