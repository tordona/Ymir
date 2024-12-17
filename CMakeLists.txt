cmake_minimum_required(VERSION 3.29)
project(satemu VERSION 0.1.0)

include(cmake/AddVersionDefines.cmake)
include(cmake/CopyRuntimeDLLs.cmake)

# CMake project structured adapted from Alex Reinking's SharedStaticStarter
# GitHub: https://github.com/alexreinking/SharedStaticStarter
# Blog: https://alexreinking.com/blog/building-a-dual-shared-and-static-library-with-cmake.html

## Determine if this is a top-level build or included as a subproject
string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}" is_top_level)

option(satemu_INCLUDE_PACKAGING "Include packaging rules for satemu" "${is_top_level}")
option(satemu_ENABLE_IPO "Enable IPO / LTO for satemu" ON)

## C++ language configuration boilerplate
if (NOT DEFINED CMAKE_CXX_VISIBILITY_PRESET AND
    NOT DEFINED CMAKE_VISIBILITY_INLINES_HIDDEN)
  set(CMAKE_CXX_VISIBILITY_PRESET hidden)
  set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)
endif ()

## Let satemu_SHARED_LIBS override BUILD_SHARED_LIBS
if (DEFINED satemu_SHARED_LIBS)
    set(BUILD_SHARED_LIBS "${satemu_SHARED_LIBS}")
endif ()

## Configure Visual Studio solution
if (MSVC)
    include(cmake/VSHelpers.cmake)

    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif ()

## Include third-party libraries
add_subdirectory(vendor)

## Include project libraries and apps
add_subdirectory(libs)
add_subdirectory(apps)

## Include the install rules if the user wanted them (included by default when top-level)
if (satemu_INCLUDE_PACKAGING)
    add_subdirectory(packaging)
endif ()

## Configure Visual Studio solution
if (MSVC)
    set_property(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}" PROPERTY VS_STARTUP_PROJECT satemu-sandbox)
endif ()
