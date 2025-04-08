## Create the library target
add_library(satemu-core
    include/satemu/satemu.hpp
    include/satemu/version.hpp

    include/satemu/core/configuration.hpp
    include/satemu/core/configuration_defs.hpp
    include/satemu/core/scheduler.hpp
    include/satemu/core/types.hpp

    include/satemu/debug/scu_tracer_base.hpp
    include/satemu/debug/sh2_tracer_base.hpp

    include/satemu/hw/hw_defs.hpp

    include/satemu/hw/cart/cart_base.hpp
    include/satemu/hw/cart/cart_impl_bup.hpp
    include/satemu/hw/cart/cart_impl_dram.hpp
    include/satemu/hw/cart/cart_impl_none.hpp
    include/satemu/hw/cart/cart_slot.hpp

    include/satemu/hw/cdblock/cdblock.hpp
    include/satemu/hw/cdblock/cdblock_buffer.hpp
    include/satemu/hw/cdblock/cdblock_callbacks.hpp
    include/satemu/hw/cdblock/cdblock_defs.hpp
    include/satemu/hw/cdblock/cdblock_filter.hpp

    include/satemu/hw/m68k/m68k.hpp
    include/satemu/hw/m68k/m68k_decode.hpp
    include/satemu/hw/m68k/m68k_defs.hpp

    include/satemu/hw/scsp/scsp.hpp
    include/satemu/hw/scsp/scsp_callbacks.hpp
    include/satemu/hw/scsp/scsp_defs.hpp
    include/satemu/hw/scsp/scsp_dsp.hpp
    include/satemu/hw/scsp/scsp_slot.hpp
    include/satemu/hw/scsp/scsp_timer.hpp

    include/satemu/hw/scu/scu.hpp
    include/satemu/hw/scu/scu_callbacks.hpp
    include/satemu/hw/scu/scu_defs.hpp
    include/satemu/hw/scu/scu_dma.hpp
    include/satemu/hw/scu/scu_dsp.hpp
    include/satemu/hw/scu/scu_dsp_disasm.hpp

    include/satemu/hw/sh2/sh2.hpp
    include/satemu/hw/sh2/sh2_bsc.hpp
    include/satemu/hw/sh2/sh2_cache.hpp
    include/satemu/hw/sh2/sh2_callbacks.hpp
    include/satemu/hw/sh2/sh2_decode.hpp
    include/satemu/hw/sh2/sh2_disasm.hpp
    include/satemu/hw/sh2/sh2_divu.hpp
    include/satemu/hw/sh2/sh2_dmac.hpp
    include/satemu/hw/sh2/sh2_excpt.hpp
    include/satemu/hw/sh2/sh2_frt.hpp
    include/satemu/hw/sh2/sh2_intc.hpp
    include/satemu/hw/sh2/sh2_power.hpp
    include/satemu/hw/sh2/sh2_regs.hpp
    include/satemu/hw/sh2/sh2_sci.hpp
    include/satemu/hw/sh2/sh2_ubc.hpp
    include/satemu/hw/sh2/sh2_wdt.hpp

    include/satemu/hw/smpc/rtc.hpp
    include/satemu/hw/smpc/smpc.hpp
    include/satemu/hw/smpc/smpc_callbacks.hpp
    
    include/satemu/hw/smpc/peripheral/peripheral_base.hpp
    include/satemu/hw/smpc/peripheral/peripheral_defs.hpp
    include/satemu/hw/smpc/peripheral/peripheral_impl_null.hpp
    include/satemu/hw/smpc/peripheral/peripheral_impl_standard_pad.hpp
    include/satemu/hw/smpc/peripheral/peripheral_port.hpp

    include/satemu/hw/vdp/vdp.hpp
    include/satemu/hw/vdp/vdp_callbacks.hpp
    include/satemu/hw/vdp/vdp_defs.hpp
    include/satemu/hw/vdp/vdp1_defs.hpp
    include/satemu/hw/vdp/vdp1_regs.hpp
    include/satemu/hw/vdp/vdp2_defs.hpp
    include/satemu/hw/vdp/vdp2_regs.hpp

    include/satemu/media/disc.hpp
    include/satemu/media/filesystem.hpp
    include/satemu/media/frame_address.hpp
    include/satemu/media/iso9660.hpp
    include/satemu/media/media_defs.hpp
    include/satemu/media/saturn_header.hpp
    include/satemu/media/subheader.hpp
    
    include/satemu/media/loader/loader.hpp
    include/satemu/media/loader/loader_bin_cue.hpp
    include/satemu/media/loader/loader_img_ccd_sub.hpp
    include/satemu/media/loader/loader_iso.hpp
    include/satemu/media/loader/loader_mdf_mds.hpp

    include/satemu/media/binary_reader/binary_reader.hpp
    include/satemu/media/binary_reader/binary_reader_file.hpp
    include/satemu/media/binary_reader/binary_reader_impl.hpp
    include/satemu/media/binary_reader/binary_reader_mem.hpp
    include/satemu/media/binary_reader/binary_reader_mmap.hpp
    include/satemu/media/binary_reader/binary_reader_subview.hpp

    include/satemu/sys/backup_ram.hpp
    include/satemu/sys/backup_ram_defs.hpp
    include/satemu/sys/clocks.hpp
    include/satemu/sys/memory.hpp
    include/satemu/sys/bus.hpp
    include/satemu/sys/saturn.hpp
    include/satemu/sys/system.hpp
    include/satemu/sys/system_callbacks.hpp
    include/satemu/sys/system_features.hpp
    include/satemu/sys/sys_ops.hpp
    
    include/satemu/util/arith_ops.hpp
    include/satemu/util/backup_datetime.hpp
    include/satemu/util/bit_ops.hpp
    include/satemu/util/bitmask_enum.hpp
    include/satemu/util/callback.hpp
    include/satemu/util/compiler_info.hpp
    include/satemu/util/data_ops.hpp
    include/satemu/util/date_time.hpp
    include/satemu/util/dev_log.hpp
    include/satemu/util/event.hpp
    include/satemu/util/function_info.hpp
    include/satemu/util/inline.hpp
    include/satemu/util/process.hpp
    include/satemu/util/scope_guard.hpp
    include/satemu/util/size_ops.hpp
    include/satemu/util/thread_name.hpp
    include/satemu/util/type_traits_ex.hpp
    include/satemu/util/unreachable.hpp


    src/satemu/satemu.cpp

    src/satemu/core/configuration.cpp

    src/satemu/hw/cart/cart_slot.cpp
    src/satemu/hw/cart/cart_impl_bup.cpp

    src/satemu/hw/cdblock/cdblock.cpp
    src/satemu/hw/cdblock/cdblock_devlog.hpp
    src/satemu/hw/cdblock/cdblock_partition_manager.cpp
    
    src/satemu/hw/m68k/m68k.cpp
    src/satemu/hw/m68k/m68k_decode.cpp
    
    src/satemu/hw/scsp/scsp.cpp
    src/satemu/hw/scsp/scsp_dsp.cpp
    src/satemu/hw/scsp/scsp_slot.cpp
    
    src/satemu/hw/scu/scu.cpp
    src/satemu/hw/scu/scu_devlog.hpp
    src/satemu/hw/scu/scu_dsp.cpp
    src/satemu/hw/scu/scu_dsp_disasm.cpp

    src/satemu/hw/smpc/rtc.cpp
    src/satemu/hw/smpc/smpc.cpp

    src/satemu/hw/smpc/peripheral/peripheral_impl_standard_pad.cpp

    src/satemu/hw/sh2/sh2.cpp
    
    src/satemu/hw/sh2/sh2_decode.cpp
    src/satemu/hw/sh2/sh2_disasm.cpp

    src/satemu/hw/vdp/slope.hpp
    src/satemu/hw/vdp/vdp.cpp

    src/satemu/media/filesystem.cpp
    src/satemu/media/saturn_header.cpp

    src/satemu/media/loader/loader.cpp
    src/satemu/media/loader/loader_bin_cue.cpp
    src/satemu/media/loader/loader_img_ccd_sub.cpp
    src/satemu/media/loader/loader_iso.cpp
    src/satemu/media/loader/loader_mdf_mds.cpp

    src/satemu/sys/backup_ram.cpp
    src/satemu/sys/memory.cpp
    src/satemu/sys/saturn.cpp

    src/satemu/util/backup_datetime.cpp
    src/satemu/util/constexpr_for.hpp
    src/satemu/util/date_time.cpp
    src/satemu/util/process.cpp
)
add_library(satemu::satemu-core ALIAS satemu-core)
set_target_properties(satemu-core PROPERTIES
                      VERSION ${satemu_core_VERSION}
                      SOVERSION ${satemu_core_VERSION_MAJOR})
target_include_directories(satemu-core
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
)
target_compile_features(satemu-core PUBLIC cxx_std_20)

## Add dependencies
target_link_libraries(satemu-core PUBLIC fmt mio concurrentqueue)

# Build precompiled header
target_precompile_headers(satemu-core
    PUBLIC
        <algorithm>
        <array>
        <atomic>
        <bit>
        <cassert>
        <cmath>
        <concepts>
        <condition_variable>
        <cstddef>
        <cstdint>
        <deque>
        <filesystem>
        <fstream>
        <functional>
        <initializer_list>
        <iosfwd>
        <iostream>
        <limits>
        <memory>
        <mutex>
        <new>
        <numeric>
        <optional>
        <queue>
        <set>
        <span>
        <sstream>
        <string>
        <thread>
        <type_traits>
        <unordered_map>
        <unordered_set>
        <utility>
        <variant>
        <vector>

        <fmt/format.h>
        <fmt/xchar.h>

        <mio/mmap.hpp>
)

## Define version macros
target_add_version_defines(satemu-core)

## Define additional macros
target_compile_definitions(satemu-core PUBLIC "satemu_ENABLE_DEVLOG=$<BOOL:${satemu_ENABLE_DEVLOG}>")
target_compile_definitions(satemu-core PUBLIC "TOML_EXCEPTIONS=0")

## Generate the export header and attach it to the target
include(GenerateExportHeader)
generate_export_header(satemu-core EXPORT_FILE_NAME include/satemu/export.h)
target_compile_definitions(satemu-core PUBLIC "$<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:satemu_core_STATIC_DEFINE>")
target_include_directories(satemu-core PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")

## Enable LTO if supported
include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT IPO_ERROR)

if(IPO_SUPPORTED)
    message(STATUS "IPO / LTO supported")
    if(satemu_ENABLE_IPO)
        message(STATUS "Enabling IPO / LTO for satemu-core")
        set_property(TARGET satemu-core PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
else()
    message(STATUS "IPO / LTO not supported: ${IPO_ERROR}")
endif()

## Apply performance options
if (satemu_AVX2)
    target_compile_definitions(satemu-core PUBLIC ${PROJECT_NAME}_AVX2=1)
    if (MSVC)
        target_compile_options(satemu-core PUBLIC "/arch:AVX2")
    else ()
        target_compile_options(satemu-core PUBLIC "-mavx2")
        target_compile_options(satemu-core PUBLIC "-mfma")
        target_compile_options(satemu-core PUBLIC "-mbmi")
    endif ()
endif ()

## Enable unity build
#set_target_properties(satemu-core PROPERTIES UNITY_BUILD ON)
#set_target_properties(satemu-core PROPERTIES UNITY_BUILD_BATCH_SIZE 0)

## Configure Visual Studio filters
if (MSVC)
    vs_set_filters(TARGET satemu-core)
    set_target_properties(satemu-core PROPERTIES FOLDER "satemu")
endif ()

## Include the install rules if the user wanted them
if (satemu_INCLUDE_PACKAGING)
    add_subdirectory(packaging)
endif ()
