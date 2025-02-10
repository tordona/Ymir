## Create the executable target
add_executable(satemu-sdl3
    src/main.cpp
)
add_executable(satemu::satemu-sdl3 ALIAS satemu-sdl3)
set_target_properties(satemu-sdl3 PROPERTIES
                      VERSION ${satemu_VERSION}
                      SOVERSION ${satemu_VERSION_MAJOR})
target_link_libraries(satemu-sdl3 PRIVATE satemu::satemu-frontend-base SDL3::SDL3)
target_compile_features(satemu-sdl3 PUBLIC cxx_std_20)

## Add dependencies
target_link_libraries(satemu-sdl3 PRIVATE fmt)

cmrk_copy_runtime_dlls(satemu-sdl3)

## Configure Visual Studio solution
if (MSVC)
    vs_set_filters(TARGET satemu-sdl3)
    set_target_properties(satemu-sdl3 PROPERTIES FOLDER "satemu")
endif ()

## Include the install rules if requested
if (satemu_INCLUDE_PACKAGING)
    add_subdirectory(packaging)
endif ()
