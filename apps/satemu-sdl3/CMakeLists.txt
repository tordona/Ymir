## Create the executable target
add_executable(satemu-sdl3
    src/main.cpp
    src/winmain.cpp

    src/app/app.cpp
    src/app/app.hpp
    src/app/audio_system.cpp
    src/app/audio_system.hpp
    
    src/app/cmdline_opts.hpp

    src/app/debug/sh2_tracer.cpp
    src/app/debug/sh2_tracer.hpp
    src/app/debug/scu_tracer.cpp
    src/app/debug/scu_tracer.hpp

    src/util/rom_loader.cpp
    src/util/rom_loader.hpp
 )
add_executable(satemu::satemu-sdl3 ALIAS satemu-sdl3)
set_target_properties(satemu-sdl3 PROPERTIES
                      VERSION ${satemu_VERSION}
                      SOVERSION ${satemu_VERSION_MAJOR})
target_include_directories(satemu-sdl3
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
)
target_link_libraries(satemu-sdl3 PRIVATE satemu::satemu-frontend-base SDL3::SDL3 cxxopts imgui::imgui)
target_compile_features(satemu-sdl3 PUBLIC cxx_std_20)

## Create embedded resource library
cmrc_add_resource_library(satemu-sdl-rc
    ALIAS satemu-sdl3::rc
    NAMESPACE satemu_sdl3_rc
    WHENCE res/embed
    res/embed/fonts/SplineSans-Bold.ttf
    res/embed/fonts/SplineSans-Medium.ttf
    res/embed/fonts/SplineSansMono-Bold.ttf
    res/embed/fonts/SplineSansMono-Medium.ttf
    res/embed/fonts/ZenDots-Regular.ttf
)

## Add dependencies
target_link_libraries(satemu-sdl3 PRIVATE fmt satemu-sdl3::rc)

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

## Apply performance options
if (satemu_AVX2)
    if (MSVC)
        target_compile_options(satemu-sdl3 PUBLIC "/arch:AVX2")
    else ()
        target_compile_options(satemu-sdl3 PUBLIC "-mavx2")
        target_compile_options(satemu-sdl3 PUBLIC "-mfma")
        target_compile_options(satemu-sdl3 PUBLIC "-mbmi")
    endif ()
endif ()

## Configure Visual Studio solution
if (MSVC)
    vs_set_filters(TARGET satemu-sdl3)
    set_target_properties(satemu-sdl3 PROPERTIES FOLDER "satemu")
endif ()

## Include the install rules if requested
if (satemu_INCLUDE_PACKAGING)
    add_subdirectory(packaging)
endif ()
