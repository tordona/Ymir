## Create the executable target
add_executable(satemu-sdl3
    src/main.cpp
    src/winmain.cpp

    src/app/app.cpp
    src/app/app.hpp
    src/app/app_impl.cpp
    src/app/app_impl.hpp
    src/app/cmdline_opts.hpp

    src/util/rom_loader.cpp
    src/util/rom_loader.hpp
)
add_executable(satemu::satemu-sdl3 ALIAS satemu-sdl3)
set_target_properties(satemu-sdl3 PROPERTIES
                      VERSION ${satemu_VERSION}
                      SOVERSION ${satemu_VERSION_MAJOR})
target_link_libraries(satemu-sdl3 PRIVATE satemu::satemu-frontend-base SDL3::SDL3 cxxopts)
target_compile_features(satemu-sdl3 PUBLIC cxx_std_20)

## Add dependencies
target_link_libraries(satemu-sdl3 PRIVATE fmt)

cmrk_copy_runtime_dlls(satemu-sdl3)

## Enable LTO if supported
include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT IPO_ERROR)

if(IPO_SUPPORTED)
    message(STATUS "IPO / LTO supported")
    if(satemu_ENABLE_IPO)
        message(STATUS "Enabling IPO / LTO for satemu-sdl3")
        set_property(TARGET satemu-sdl3 PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
else()
    message(STATUS "IPO / LTO not supported: ${IPO_ERROR}")
endif()

## Configure Visual Studio solution
if (MSVC)
    vs_set_filters(TARGET satemu-sdl3)
    set_target_properties(satemu-sdl3 PROPERTIES FOLDER "satemu")
endif ()

## Include the install rules if requested
if (satemu_INCLUDE_PACKAGING)
    add_subdirectory(packaging)
endif ()
