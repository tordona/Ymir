## Create the executable target
add_executable(satemu-core-tests
    src/tests_main.cpp

    src/hw/sh2/sh2_tests.cpp
)
add_executable(satemu::satemu-core-tests ALIAS satemu-core-tests)
set_target_properties(satemu-core-tests PROPERTIES
                      VERSION ${satemu_VERSION}
                      SOVERSION ${satemu_VERSION_MAJOR})
target_link_libraries(satemu-core-tests PRIVATE satemu::satemu-core)
target_compile_features(satemu-core-tests PUBLIC cxx_std_20)

## Add dependencies
target_link_libraries(satemu-core-tests PRIVATE fmt Catch2::Catch2WithMain)

cmrk_copy_runtime_dlls(satemu-core-tests)

## Enable LTO if supported
include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT IPO_ERROR)

if(IPO_SUPPORTED)
    message(STATUS "IPO / LTO supported")
    if(satemu_ENABLE_IPO)
        message(STATUS "Enabling IPO / LTO for satemu-core-tests")
        set_property(TARGET satemu-core-tests PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
else()
    message(STATUS "IPO / LTO not supported: ${IPO_ERROR}")
endif()

## Apply performance options
if (satemu_AVX2)
    if (MSVC)
        target_compile_options(satemu-core-tests PUBLIC "/arch:AVX2")
    else ()
        target_compile_options(satemu-core-tests PUBLIC "-mavx2")
        target_compile_options(satemu-core-tests PUBLIC "-mfma")
        target_compile_options(satemu-core-tests PUBLIC "-mbmi")
    endif ()
endif ()

## Configure Visual Studio solution
if (MSVC)
    vs_set_filters(TARGET satemu-core-tests)
    set_target_properties(satemu-core-tests PROPERTIES FOLDER "satemu-tests")
endif ()

## No packaging for this project it's meant for unit tests
