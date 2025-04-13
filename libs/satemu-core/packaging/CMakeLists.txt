install(TARGETS satemu-core xxHash EXPORT satemu_Targets
        RUNTIME COMPONENT satemu_Runtime
        LIBRARY COMPONENT satemu_Runtime
        NAMELINK_COMPONENT satemu_Development
        ARCHIVE COMPONENT satemu_Development
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

get_target_property(LIB_SOURCE_DIR satemu-core SOURCE_DIR)
get_target_property(LIB_BINARY_DIR satemu-core BINARY_DIR)

install(DIRECTORY "${LIB_SOURCE_DIR}/include/"
        TYPE INCLUDE
        COMPONENT satemu_Development)

install(FILES "${LIB_BINARY_DIR}/include/satemu/export.h"
        DESTINATION include/lib/
        COMPONENT satemu_Development
)
