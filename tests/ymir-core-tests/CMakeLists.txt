## Create the executable target
add_executable(Ymir-core-tests
    src/hw/scu/scu_dsp_tests.cpp

    src/hw/sh2/sh2_disasm_tests.cpp
    src/hw/sh2/sh2_divu_tests.cpp
    src/hw/sh2/sh2_intc_tests.cpp
    src/hw/sh2/sh2_macwl_tests.cpp
)
add_executable(Ymir::Ymir-core-tests ALIAS Ymir-core-tests)
set_target_properties(Ymir-core-tests PROPERTIES
                      VERSION ${Ymir_VERSION}
                      SOVERSION ${Ymir_VERSION_MAJOR})
target_link_libraries(Ymir-core-tests PRIVATE Ymir::Ymir-core)
target_compile_features(Ymir-core-tests PUBLIC cxx_std_20)

## Add dependencies
target_link_libraries(Ymir-core-tests PRIVATE fmt Catch2::Catch2WithMain)

cmrk_copy_runtime_dlls(Ymir-core-tests)

## Enable LTO if supported
include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT IPO_ERROR)

if (IPO_SUPPORTED AND Ymir_ENABLE_IPO)
    message(STATUS "Enabling IPO / LTO for Ymir-core-tests")
    set_property(TARGET Ymir-core-tests PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

## Apply performance options
if (Ymir_AVX2)
    if (MSVC)
        target_compile_options(Ymir-core-tests PUBLIC "/arch:AVX2")
    else ()
        target_compile_options(Ymir-core-tests PUBLIC "-mavx2")
        target_compile_options(Ymir-core-tests PUBLIC "-mfma")
        target_compile_options(Ymir-core-tests PUBLIC "-mbmi")
    endif ()
endif ()

## Configure Visual Studio solution
if (MSVC)
    vs_set_filters(TARGET Ymir-core-tests)
    set_target_properties(Ymir-core-tests PROPERTIES FOLDER "Ymir-tests")
endif ()

## Register Catch2 test with CTest
catch_discover_tests(Ymir-core-tests)

## No packaging for this project it's meant for unit tests
