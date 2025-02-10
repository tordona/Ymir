install(TARGETS satemu-sdl3 EXPORT satemu_Targets
        RUNTIME COMPONENT satemu_Runtime
        LIBRARY COMPONENT satemu_Runtime
        NAMELINK_COMPONENT satemu_Development
        ARCHIVE COMPONENT satemu_Development
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
