## Create the executable target
add_executable(satemu-sdl3
    src/main.cpp
    src/winmain.cpp

    src/app/app.cpp
    src/app/app.hpp
    src/app/audio_system.cpp
    src/app/audio_system.hpp
    src/app/cmdline_opts.hpp
    src/app/profile.cpp
    src/app/profile.hpp
    src/app/settings.hpp
    src/app/settings.cpp
    src/app/shared_context.hpp

    src/app/debug/scu_tracer.cpp
    src/app/debug/scu_tracer.hpp
    src/app/debug/sh2_tracer.cpp
    src/app/debug/sh2_tracer.hpp
    
    src/app/events/emu_debug_event_factory.cpp
    src/app/events/emu_debug_event_factory.hpp
    src/app/events/emu_event.hpp
    src/app/events/emu_event_factory.cpp
    src/app/events/emu_event_factory.hpp
    src/app/events/gui_event.hpp
    src/app/events/gui_event_factory.hpp

    src/app/input/input_backend_sdl3.cpp
    src/app/input/input_backend_sdl3.hpp
    src/app/input/input_context.cpp
    src/app/input/input_context.hpp
    src/app/input/input_defs.hpp
    src/app/input/input_handler.cpp
    src/app/input/input_handler.hpp

    src/app/ui/window_base.cpp
    src/app/ui/window_base.hpp

    src/app/ui/state/debug/memory_viewer_state.hpp

    src/app/ui/views/backup_memory_view.cpp
    src/app/ui/views/backup_memory_view.hpp

    src/app/ui/views/debug/debug_output_view.cpp
    src/app/ui/views/debug/debug_output_view.hpp
    src/app/ui/views/debug/scu_dma_registers_view.cpp
    src/app/ui/views/debug/scu_dma_registers_view.hpp
    src/app/ui/views/debug/scu_dma_state_view.cpp
    src/app/ui/views/debug/scu_dma_state_view.hpp
    src/app/ui/views/debug/scu_dma_trace_view.cpp
    src/app/ui/views/debug/scu_dma_trace_view.hpp
    src/app/ui/views/debug/scu_dsp_data_ram_view.cpp
    src/app/ui/views/debug/scu_dsp_data_ram_view.hpp
    src/app/ui/views/debug/scu_dsp_disassembly_view.cpp
    src/app/ui/views/debug/scu_dsp_disassembly_view.hpp
    src/app/ui/views/debug/scu_dsp_dma_registers_view.cpp
    src/app/ui/views/debug/scu_dsp_dma_registers_view.hpp
    src/app/ui/views/debug/scu_dsp_dma_trace_view.cpp
    src/app/ui/views/debug/scu_dsp_dma_trace_view.hpp
    src/app/ui/views/debug/scu_dsp_registers_view.cpp
    src/app/ui/views/debug/scu_dsp_registers_view.hpp
    src/app/ui/views/debug/scu_interrupts_view.cpp
    src/app/ui/views/debug/scu_interrupts_view.hpp
    src/app/ui/views/debug/scu_interrupt_trace_view.cpp
    src/app/ui/views/debug/scu_interrupt_trace_view.hpp
    src/app/ui/views/debug/scu_registers_view.cpp
    src/app/ui/views/debug/scu_registers_view.hpp
    src/app/ui/views/debug/scu_timers_view.cpp
    src/app/ui/views/debug/scu_timers_view.hpp
    src/app/ui/views/debug/sh2_cache_entries_view.cpp
    src/app/ui/views/debug/sh2_cache_entries_view.hpp
    src/app/ui/views/debug/sh2_cache_register_view.cpp
    src/app/ui/views/debug/sh2_cache_register_view.hpp
    src/app/ui/views/debug/sh2_disassembly_view.cpp
    src/app/ui/views/debug/sh2_disassembly_view.hpp
    src/app/ui/views/debug/sh2_divu_registers_view.cpp
    src/app/ui/views/debug/sh2_divu_registers_view.hpp
    src/app/ui/views/debug/sh2_divu_statistics_view.cpp
    src/app/ui/views/debug/sh2_divu_statistics_view.hpp
    src/app/ui/views/debug/sh2_divu_trace_view.cpp
    src/app/ui/views/debug/sh2_divu_trace_view.hpp
    src/app/ui/views/debug/sh2_dmac_channel_view.cpp
    src/app/ui/views/debug/sh2_dmac_channel_view.hpp
    src/app/ui/views/debug/sh2_dmac_registers_view.cpp
    src/app/ui/views/debug/sh2_dmac_registers_view.hpp
    src/app/ui/views/debug/sh2_dmac_trace_view.cpp
    src/app/ui/views/debug/sh2_dmac_trace_view.hpp
    src/app/ui/views/debug/sh2_frt_view.cpp
    src/app/ui/views/debug/sh2_frt_view.hpp
    src/app/ui/views/debug/sh2_interrupt_trace_view.cpp
    src/app/ui/views/debug/sh2_interrupt_trace_view.hpp
    src/app/ui/views/debug/sh2_interrupts_view.cpp
    src/app/ui/views/debug/sh2_interrupts_view.hpp
    src/app/ui/views/debug/sh2_registers_view.cpp
    src/app/ui/views/debug/sh2_registers_view.hpp
    src/app/ui/views/debug/sh2_wdt_view.cpp
    src/app/ui/views/debug/sh2_wdt_view.hpp

    src/app/ui/views/settings/audio_settings_view.cpp
    src/app/ui/views/settings/audio_settings_view.hpp
    src/app/ui/views/settings/general_settings_view.cpp
    src/app/ui/views/settings/general_settings_view.hpp
    src/app/ui/views/settings/input_settings_view.cpp
    src/app/ui/views/settings/input_settings_view.hpp
    src/app/ui/views/settings/settings_view_base.cpp
    src/app/ui/views/settings/settings_view_base.hpp
    src/app/ui/views/settings/system_settings_view.cpp
    src/app/ui/views/settings/system_settings_view.hpp
    src/app/ui/views/settings/video_settings_view.cpp
    src/app/ui/views/settings/video_settings_view.hpp

    src/app/ui/widgets/cartridge_widgets.hpp
    src/app/ui/widgets/cartridge_widgets.cpp
    src/app/ui/widgets/common_widgets.cpp
    src/app/ui/widgets/common_widgets.hpp
    src/app/ui/widgets/datetime_widgets.cpp
    src/app/ui/widgets/datetime_widgets.hpp
    src/app/ui/widgets/peripheral_widgets.cpp
    src/app/ui/widgets/peripheral_widgets.hpp
    src/app/ui/widgets/system_widgets.cpp
    src/app/ui/widgets/system_widgets.hpp

    src/app/ui/windows/about_window.cpp
    src/app/ui/windows/about_window.hpp
    src/app/ui/windows/backup_ram_manager_window.cpp
    src/app/ui/windows/backup_ram_manager_window.hpp
    src/app/ui/windows/settings_window.cpp
    src/app/ui/windows/settings_window.hpp
    src/app/ui/windows/system_state_window.cpp
    src/app/ui/windows/system_state_window.hpp

    src/app/ui/windows/debug/debug_output_window.cpp
    src/app/ui/windows/debug/debug_output_window.hpp
    src/app/ui/windows/debug/memory_viewer_window.cpp
    src/app/ui/windows/debug/memory_viewer_window.hpp
    src/app/ui/windows/debug/scu_dma_window.cpp
    src/app/ui/windows/debug/scu_dma_window.hpp
    src/app/ui/windows/debug/scu_dma_trace_window.cpp
    src/app/ui/windows/debug/scu_dma_trace_window.hpp
    src/app/ui/windows/debug/scu_dsp_window.cpp
    src/app/ui/windows/debug/scu_dsp_window.hpp
    src/app/ui/windows/debug/scu_interrupt_trace_window.cpp
    src/app/ui/windows/debug/scu_interrupt_trace_window.hpp
    src/app/ui/windows/debug/scu_registers_window.cpp
    src/app/ui/windows/debug/scu_registers_window.hpp
    src/app/ui/windows/debug/scu_window_set.hpp
    src/app/ui/windows/debug/sh2_cache_window.cpp
    src/app/ui/windows/debug/sh2_cache_window.hpp
    src/app/ui/windows/debug/sh2_debugger_window.cpp
    src/app/ui/windows/debug/sh2_debugger_window.hpp
    src/app/ui/windows/debug/sh2_divu_window.cpp
    src/app/ui/windows/debug/sh2_divu_window.hpp
    src/app/ui/windows/debug/sh2_dmac_trace_window.cpp
    src/app/ui/windows/debug/sh2_dmac_trace_window.hpp
    src/app/ui/windows/debug/sh2_dmac_window.cpp
    src/app/ui/windows/debug/sh2_dmac_window.hpp
    src/app/ui/windows/debug/sh2_interrupt_trace_window.cpp
    src/app/ui/windows/debug/sh2_interrupt_trace_window.hpp
    src/app/ui/windows/debug/sh2_interrupts_window.cpp
    src/app/ui/windows/debug/sh2_interrupts_window.hpp
    src/app/ui/windows/debug/sh2_timers_window.cpp
    src/app/ui/windows/debug/sh2_timers_window.hpp
    src/app/ui/windows/debug/sh2_window_base.cpp
    src/app/ui/windows/debug/sh2_window_base.hpp
    src/app/ui/windows/debug/sh2_window_set.hpp

    src/util/ring_buffer.hpp
    src/util/rom_loader.cpp
    src/util/rom_loader.hpp
    src/util/sdl_file_dialog.hpp
)
add_executable(satemu::satemu-sdl3 ALIAS satemu-sdl3)
set_target_properties(satemu-sdl3 PROPERTIES
                      VERSION ${satemu_VERSION}
                      SOVERSION ${satemu_VERSION_MAJOR})
target_include_directories(satemu-sdl3
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
)
target_link_libraries(satemu-sdl3 PRIVATE
    satemu::satemu-core
    SDL3::SDL3
    cxxopts
    imgui::imgui
    tomlplusplus::tomlplusplus
)
target_compile_features(satemu-sdl3 PUBLIC cxx_std_20)

## Create embedded resource library
cmrc_add_resource_library(satemu-sdl3-rc
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

## Make it a Win32 executable (without a console window) on Windows
if (WIN32 AND NOT satemu_ENABLE_DEVLOG)
    set_target_properties(satemu-sdl3 PROPERTIES WIN32_EXECUTABLE ON)
endif ()

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

## Define additional macros
target_compile_definitions(satemu-sdl3 PUBLIC satemu_ENABLE_IMGUI_DEMO=$<BOOL:${satemu_ENABLE_IMGUI_DEMO}>)
target_compile_definitions(satemu-sdl3 PUBLIC satemu_ORGANIZATION_NAME="StrikerX3")
target_compile_definitions(satemu-sdl3 PUBLIC satemu_APP_NAME="${PROJECT_NAME}")

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
    set_target_properties(satemu-sdl3-rc PROPERTIES FOLDER "satemu")
endif ()

## Include the install rules if requested
if (satemu_INCLUDE_PACKAGING)
    add_subdirectory(packaging)
endif ()
