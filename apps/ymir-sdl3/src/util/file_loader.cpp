#include <util/file_loader.hpp>

#include <fstream>

namespace util {

std::vector<uint8> LoadFile(std::filesystem::path romPath) {
    std::vector<uint8> data{};
    std::ifstream stream{romPath, std::ios::binary | std::ios::ate};
    if (stream.is_open()) {
        auto size = stream.tellg();
        stream.seekg(0, std::ios::beg);
        data.resize(size);
        stream.read(reinterpret_cast<char *>(data.data()), size);
    }
    return data;
}

} // namespace util
