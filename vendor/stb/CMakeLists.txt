add_library(stb INTERFACE
    stb/stb_image.h
    stb/stb_image_write.h
)

add_library(stb::stb ALIAS stb)
target_include_directories(stb
    INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/stb>"
)
target_compile_features(stb INTERFACE cxx_std_20)

## Apply performance options
if (Ymir_AVX2)
    if (MSVC)
        target_compile_options(stb INTERFACE "/arch:AVX2")
    else ()
        target_compile_options(stb INTERFACE "-mavx2")
        target_compile_options(stb INTERFACE "-mfma")
        target_compile_options(stb INTERFACE "-mbmi")
    endif ()
endif ()
