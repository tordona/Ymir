#include "profile.hpp"

#include <SDL3/SDL_filesystem.h>

namespace app {

// Must match the order listed in the ProfilePath enum.
const std::filesystem::path kPathSuffixes[] = {
    "",                                           // Root
    std::filesystem::path("roms") / "ipl",        // IPLROMImages
    std::filesystem::path("roms") / "cart",       // CartROMImages
    "backup",                                     // BackupMemory
    std::filesystem::path("backup") / "exported", // ExportedBackups
    "state",                                      // PersistentState
    "savestates",                                 // SaveStates
    "dumps",                                      // Dumps
};

Profile::Profile() {
    UsePortableProfilePath();
}

void Profile::UseUserProfilePath() {
    char *path = SDL_GetPrefPath(Ymir_ORGANIZATION_NAME, Ymir_APP_NAME);
    m_profilePath = path;
    SDL_free(path);
}

void Profile::UsePortableProfilePath() {
    char *path = SDL_GetCurrentDirectory();
    m_profilePath = path;
    SDL_free(path);
}

void Profile::UseProfilePath(std::filesystem::path path) {
    m_profilePath = path;
}

bool Profile::CheckFolders() const {
    for (auto &suffix : kPathSuffixes) {
        if (!std::filesystem::is_directory(m_profilePath / suffix)) {
            return false;
        }
    }
    return true;
}

bool Profile::CreateFolders(std::error_code &error) {
    error.clear();

    for (auto &suffix : kPathSuffixes) {
        std::filesystem::create_directories(m_profilePath / suffix, error);
        if (error) {
            return false;
        }
    }
    return true;
}

std::filesystem::path Profile::GetPath(ProfilePath path) const {
    const auto index = static_cast<size_t>(path);
    if (index < std::size(kPathSuffixes)) {
        return m_profilePath / kPathSuffixes[index] / "";
    } else {
        return m_profilePath / "";
    }
}

} // namespace app
