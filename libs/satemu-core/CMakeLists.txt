## Create the library target
add_library(satemu-core
    include/satemu/satemu.hpp

    src/satemu/satemu.cpp
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

## Build precompiled header
#target_precompile_headers(satemu-core
#    PUBLIC
#        <string>
#)

## Define version macros
target_add_version_defines(satemu-core)

## Generate the export header and attach it to the target
include(GenerateExportHeader)
generate_export_header(satemu-core EXPORT_FILE_NAME include/satemu/export.h)
target_compile_definitions(satemu-core PUBLIC "$<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:satemu_core_STATIC_DEFINE>")
target_include_directories(satemu-core PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")

## Configure Visual Studio filters
if (MSVC)
    vs_set_filters(TARGET satemu-core)
    set_target_properties(satemu-core PROPERTIES FOLDER "satemu")
endif ()

## Include the install rules if the user wanted them
if (satemu_INCLUDE_PACKAGING)
    add_subdirectory(packaging)
endif ()
