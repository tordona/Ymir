## Create the library target
add_library(satemu-core
    include/satemu/core_types.hpp
    include/satemu/satemu.hpp

    include/satemu/hw/hw_defs.hpp

    include/satemu/hw/cdblock/cdblock.hpp
    include/satemu/hw/cdblock/cdblock_defs.hpp

    include/satemu/hw/m68k/m68k.hpp
    include/satemu/hw/m68k/m68k_decode.hpp
    include/satemu/hw/m68k/m68k_defs.hpp

    include/satemu/hw/scsp/scsp.hpp
    include/satemu/hw/scsp/scsp_defs.hpp

    include/satemu/hw/scu/scu.hpp
    include/satemu/hw/scu/scu_defs.hpp

    include/satemu/hw/sh2/sh2.hpp
    include/satemu/hw/sh2/sh2_block.hpp
    include/satemu/hw/sh2/sh2_bus.hpp
    include/satemu/hw/sh2/sh2_bus_defs.hpp
    include/satemu/hw/sh2/sh2_defs.hpp

    include/satemu/hw/smpc/smpc.hpp
    include/satemu/hw/smpc/smpc_defs.hpp

    include/satemu/hw/vdp/vdp.hpp
    include/satemu/hw/vdp/vdp_defs.hpp
    include/satemu/hw/vdp/vdp1_defs.hpp
    include/satemu/hw/vdp/vdp2_defs.hpp

    include/satemu/media/disc.hpp
    include/satemu/media/filesystem.hpp
    include/satemu/media/frame_address.hpp
    include/satemu/media/iso9660.hpp
    
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

    include/satemu/sys/sys.hpp
    
    include/satemu/util/bit_ops.hpp
    include/satemu/util/callback.hpp
    include/satemu/util/data_ops.hpp
    include/satemu/util/inline.hpp
    include/satemu/util/scope_guard.hpp
    include/satemu/util/size_ops.hpp
    include/satemu/util/thread_name.hpp
    include/satemu/util/unreachable.hpp


    src/satemu/satemu.cpp

    src/satemu/hw/cdblock/cdblock.cpp
    
    src/satemu/hw/m68k/m68k.cpp
    src/satemu/hw/m68k/m68k_decode.cpp
    
    src/satemu/hw/scsp/scsp.cpp
    
    src/satemu/hw/scu/scu.cpp

    src/satemu/hw/smpc/smpc.cpp

    src/satemu/hw/sh2/sh2.cpp
    src/satemu/hw/sh2/sh2_bus.cpp

    src/satemu/hw/vdp/slope.hpp
    src/satemu/hw/vdp/vdp.cpp

    src/satemu/media/filesystem.cpp

    src/satemu/media/loader/loader.cpp
    src/satemu/media/loader/loader_bin_cue.cpp
    src/satemu/media/loader/loader_img_ccd_sub.cpp
    src/satemu/media/loader/loader_iso.cpp
    src/satemu/media/loader/loader_mdf_mds.cpp

    src/satemu/sys/sys.cpp

    src/satemu/util/constexpr_for.hpp
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
target_link_libraries(satemu-core PUBLIC fmt mio)

# Build precompiled header
target_precompile_headers(satemu-core
    PUBLIC
        <algorithm>
        <array>
        <bit>
        <cassert>
        <cmath>
        <concepts>
        <cstddef>
        <cstdint>
        <filesystem>
        <fstream>
        <limits>
        <memory>
        <set>
        <span>
        <sstream>
        <type_traits>
        <unordered_map>
        <utility>
        <vector>

        <fmt/format.h>
        <fmt/xchar.h>

         <mio/mmap.hpp>
)

## Define version macros
target_add_version_defines(satemu-core)

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


## Configure Visual Studio filters
if (MSVC)
    vs_set_filters(TARGET satemu-core)
    set_target_properties(satemu-core PROPERTIES FOLDER "satemu")
endif ()

## Include the install rules if the user wanted them
if (satemu_INCLUDE_PACKAGING)
    add_subdirectory(packaging)
endif ()
