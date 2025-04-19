## Create the executable target
add_executable(Ymir-sandbox
    src/main.cpp
)
add_executable(Ymir::Ymir-sandbox ALIAS Ymir-sandbox)
set_target_properties(Ymir-sandbox PROPERTIES
                      VERSION ${Ymir_VERSION}
                      SOVERSION ${Ymir_VERSION_MAJOR})
target_include_directories(Ymir-sandbox
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
)
target_link_libraries(Ymir-sandbox PRIVATE Ymir::Ymir-core SDL3::SDL3)
target_compile_features(Ymir-sandbox PUBLIC cxx_std_20)

## Add dependencies
target_link_libraries(Ymir-sandbox PRIVATE fmt)

cmrk_copy_runtime_dlls(Ymir-sandbox)

if (IPO_SUPPORTED AND Ymir_ENABLE_IPO)
    message(STATUS "Enabling IPO / LTO for Ymir-sandbox")
    set_property(TARGET Ymir-sandbox PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

## Apply performance options
if (Ymir_AVX2)
    if (MSVC)
        target_compile_options(Ymir-sandbox PUBLIC "/arch:AVX2")
    else ()
        target_compile_options(Ymir-sandbox PUBLIC "-mavx2")
        target_compile_options(Ymir-sandbox PUBLIC "-mfma")
        target_compile_options(Ymir-sandbox PUBLIC "-mbmi")
    endif ()
endif ()

## Configure Visual Studio solution
if (MSVC)
    vs_set_filters(TARGET Ymir-sandbox)
    set_target_properties(Ymir-sandbox PROPERTIES FOLDER "Ymir")
endif ()

## No packaging for this project as it's meant to be testing grounds
