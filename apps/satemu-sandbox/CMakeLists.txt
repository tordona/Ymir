## Create the executable target
add_executable(satemu-sandbox
    src/main.cpp
)
add_executable(satemu::satemu-sandbox ALIAS satemu-sandbox)
set_target_properties(satemu-sandbox PROPERTIES
                      VERSION ${satemu_VERSION}
                      SOVERSION ${satemu_VERSION_MAJOR})
target_include_directories(satemu-sandbox
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
)
target_link_libraries(satemu-sandbox PRIVATE satemu::satemu-core SDL3::SDL3)
target_compile_features(satemu-sandbox PUBLIC cxx_std_20)

## Add dependencies
target_link_libraries(satemu-sandbox PRIVATE fmt)

cmrk_copy_runtime_dlls(satemu-sandbox)

if (IPO_SUPPORTED AND satemu_ENABLE_IPO)
    message(STATUS "Enabling IPO / LTO for satemu-sandbox")
    set_property(TARGET satemu-sandbox PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

## Apply performance options
if (satemu_AVX2)
    if (MSVC)
        target_compile_options(satemu-sandbox PUBLIC "/arch:AVX2")
    else ()
        target_compile_options(satemu-sandbox PUBLIC "-mavx2")
        target_compile_options(satemu-sandbox PUBLIC "-mfma")
        target_compile_options(satemu-sandbox PUBLIC "-mbmi")
    endif ()
endif ()

## Configure Visual Studio solution
if (MSVC)
    vs_set_filters(TARGET satemu-sandbox)
    set_target_properties(satemu-sandbox PROPERTIES FOLDER "satemu")
endif ()

## No packaging for this project as it's meant to be testing grounds
