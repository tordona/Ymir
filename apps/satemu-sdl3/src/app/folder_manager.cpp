#include "folder_manager.hpp"

#include <SDL3/SDL_filesystem.h>

namespace fs = std::filesystem;

namespace app {

// Must match the order listed in the StandardPath enum.
const fs::path kPathSuffixes[] = {
    "",                              // Root
    "bios",                          // BIOSImages
    "backup",                        // BackupMemory
    fs::path("backup") / "exported", // ExportedBackups
    "savestates",                    // SaveStates
};

FolderManager::FolderManager() {
    UsePortableProfilePath();
}

void FolderManager::UseUserProfilePath() {
    char *path = SDL_GetPrefPath(satemu_ORGANIZATION_NAME, satemu_APP_NAME);
    m_profilePath = path;
    SDL_free(path);
}

void FolderManager::UsePortableProfilePath() {
    char *path = SDL_GetCurrentDirectory();
    m_profilePath = path;
    SDL_free(path);
}

void FolderManager::UseProfilePath(fs::path path) {
    m_profilePath = path;
}

fs::path FolderManager::GetProfilePath() const {
    return m_profilePath;
}

bool FolderManager::CheckFolders() const {
    for (auto &suffix : kPathSuffixes) {
        if (!fs::is_directory(m_profilePath / suffix)) {
            return false;
        }
    }
    return true;
}

bool FolderManager::CreateFolders(std::error_code &error) {
    error.clear();

    for (auto &suffix : kPathSuffixes) {
        fmt::println("{}", (m_profilePath / suffix).string());
        fs::create_directories(m_profilePath / suffix, error);
        if (error) {
            return false;
        }
    }
    return true;
}

fs::path FolderManager::GetPath(StandardPath path) const {
    const auto index = static_cast<size_t>(path);
    if (index < std::size(kPathSuffixes)) {
        return m_profilePath / kPathSuffixes[index] / "";
    } else {
        return m_profilePath / "";
    }
}

} // namespace app
