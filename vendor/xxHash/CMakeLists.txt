add_library(xxHash
	xxHash/xxhash.c
	xxHash/xxhash.h
	xxHash/xxh_x86dispatch.c
	xxHash/xxh_x86dispatch.h
	xxHash/xxh3.h
)

add_library(xxHash::xxHash ALIAS xxHash)
target_include_directories(xxHash
    PUBLIC
		"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/xxHash>"
    PRIVATE
		"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/xxHash>"
)
target_compile_features(xxHash PUBLIC cxx_std_20)
