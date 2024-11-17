## Create the library target
add_library(satemu-frontend-base
    include/satemu/frontend/base.hpp

    src/satemu/frontend/base.cpp
)
add_library(satemu::satemu-frontend-base ALIAS satemu-frontend-base)
set_target_properties(satemu-frontend-base PROPERTIES
                      VERSION ${satemu_frontend_base_VERSION}
                      SOVERSION ${satemu_frontend_base_VERSION_MAJOR})
target_include_directories(satemu-frontend-base
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
)
target_link_libraries(satemu-frontend-base PUBLIC satemu-core)
target_compile_features(satemu-frontend-base PUBLIC cxx_std_20)

## Build precompiled header
#target_precompile_headers(satemu-frontend-base
#    PUBLIC
#        <string>
#)

## Define version macros
target_add_version_defines(satemu-frontend-base)

## Generate the export header and attach it to the target
include(GenerateExportHeader)
generate_export_header(satemu-frontend-base EXPORT_FILE_NAME include/satemu/export.h)
target_compile_definitions(satemu-frontend-base PUBLIC "$<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:satemu_frontend_base_STATIC_DEFINE>")
target_include_directories(satemu-frontend-base PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")

## Configure Visual Studio filters
if (MSVC)
    vs_set_filters(TARGET satemu-frontend-base)
    set_target_properties(satemu-frontend-base PROPERTIES FOLDER "satemu")
endif ()

## Include the install rules if the user wanted them
if (satemu_INCLUDE_PACKAGING)
    add_subdirectory(packaging)
endif ()
