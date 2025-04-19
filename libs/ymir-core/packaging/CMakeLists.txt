install(TARGETS Ymir-core xxHash EXPORT Ymir_Targets
        RUNTIME COMPONENT Ymir_Runtime
        LIBRARY COMPONENT Ymir_Runtime
        NAMELINK_COMPONENT Ymir_Development
        ARCHIVE COMPONENT Ymir_Development
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

get_target_property(LIB_SOURCE_DIR Ymir-core SOURCE_DIR)
get_target_property(LIB_BINARY_DIR Ymir-core BINARY_DIR)

install(DIRECTORY "${LIB_SOURCE_DIR}/include/"
        TYPE INCLUDE
        COMPONENT Ymir_Development)

install(FILES "${LIB_BINARY_DIR}/include/ymir/export.h"
        DESTINATION include/lib/
        COMPONENT Ymir_Development
)
