install(TARGETS ymir-sdl3 EXPORT Ymir_Targets
        RUNTIME COMPONENT Ymir_Runtime
        LIBRARY COMPONENT Ymir_Runtime
        NAMELINK_COMPONENT Ymir_Development
        ARCHIVE COMPONENT Ymir_Development
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
