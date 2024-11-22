#include <satemu/satemu.hpp>

#include <fmt/format.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <vector>

std::vector<uint8> loadFile(std::filesystem::path romPath) {
    fmt::print("Loading file {}... ", romPath.string());

    std::vector<uint8> data;
    std::ifstream stream{romPath, std::ios::binary | std::ios::ate};
    if (stream.is_open()) {
        auto size = stream.tellg();
        stream.seekg(0, std::ios::beg);
        fmt::println("{} bytes", (size_t)size);

        data.resize(size);
        stream.read(reinterpret_cast<char *>(data.data()), size);
    } else {
        fmt::println("failed!");
    }
    return data;
}

int main(int argc, char **argv) {
    fmt::println("satemu {}", satemu::version::string);
    if (argc < 2) {
        fmt::println("missing argument: rompath");
        fmt::println("    rompath   Path to Saturn BIOS ROM");
        return EXIT_FAILURE;
    }

    auto saturn = std::make_unique<satemu::Saturn>();
    {
        auto rom = loadFile(argv[1]);
        if (rom.size() != satemu::kIPLSize) {
            fmt::println("IPL ROM size mismatch: expected {} bytes, got {} bytes", satemu::kIPLSize, rom.size());
            return EXIT_FAILURE;
        }
        saturn->LoadIPL(std::span<uint8, satemu::kIPLSize>(rom));
        fmt::println("IPL ROM loaded");
    }

    saturn->Reset(true);
    auto &msh2 = saturn->MasterSH2();
    uint32 prevPC = msh2.GetPC();
    for (;;) {
        saturn->Step();
        uint32 currPC = msh2.GetPC();
        if (currPC == prevPC) {
            break;
        }
        prevPC = currPC;
    }

    return EXIT_SUCCESS;
}
