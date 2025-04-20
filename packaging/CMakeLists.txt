include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if (NOT DEFINED Ymir_INSTALL_CMAKEDIR)
   set(Ymir_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/Ymir"
       CACHE STRING "Path to Ymir CMake files")
endif ()

if (BUILD_SHARED_LIBS)
    set(type shared)
else ()
    set(type static)
endif ()

install(EXPORT Ymir_Targets
        DESTINATION "${Ymir_INSTALL_CMAKEDIR}"
        NAMESPACE ymir::
        FILE Ymir-${type}-targets.cmake
        COMPONENT Ymir_Development)

write_basic_package_version_file(
    YmirConfigVersion.cmake
    COMPATIBILITY SameMajorVersion)

install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/YmirConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/YmirConfigVersion.cmake"
        DESTINATION "${Ymir_INSTALL_CMAKEDIR}"
        COMPONENT Ymir_Development)

# TODO: add additional CPack variables here
include(CPack)
