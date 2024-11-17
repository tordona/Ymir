include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if (NOT DEFINED satemu_INSTALL_CMAKEDIR)
   set(satemu_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/satemu"
       CACHE STRING "Path to satemu CMake files")
endif ()

if (BUILD_SHARED_LIBS)
    set(type shared)
else ()
    set(type static)
endif ()

install(EXPORT satemu_Targets
        DESTINATION "${satemu_INSTALL_CMAKEDIR}"
        NAMESPACE satemu::
        FILE satemu-${type}-targets.cmake
        COMPONENT satemu_Development)

write_basic_package_version_file(
    satemuConfigVersion.cmake
    COMPATIBILITY SameMajorVersion)

install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/satemuConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/satemuConfigVersion.cmake"
        DESTINATION "${satemu_INSTALL_CMAKEDIR}"
        COMPONENT satemu_Development)

# TODO: add additional CPack variables here
include(CPack)
