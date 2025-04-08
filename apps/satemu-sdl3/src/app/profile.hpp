#pragma once

#include <filesystem>

namespace app {

enum class StandardPath {
    Root,            // Root of the profile    <profile>/
    BIOSImages,      // BIOS images            <profile>/bios/
    BackupMemory,    // Backup memory images   <profile>/backup/
    ExportedBackups, // Exported backup files  <profile>/backup/exported/
    PersistentState, // Persistent app state   <profile>/state/
    SaveStates,      // Save states            <profile>/savestates/
};

class Profile {
public:
    // Creates the folder manager pointing to the current working directory.
    Profile();

    // Uses the OS's standard user profile path.
    void UseUserProfilePath();

    // Uses the current working directory as the profile path.
    void UsePortableProfilePath();

    // Uses the specified profile path.
    void UseProfilePath(std::filesystem::path path);

    // Gets the profile path currently in use.
    std::filesystem::path GetProfilePath() const;

    // Checks if the standard folders are present in the current profile path.
    bool CheckFolders() const;

    // Creates all standard folders under the given profile path.
    // Returns true if all folders were created successfully.
    bool CreateFolders(std::error_code &error);

    // Gets the specified path relative to the profile path.
    std::filesystem::path GetPath(StandardPath path) const;

private:
    std::filesystem::path m_profilePath;
};

} // namespace app
