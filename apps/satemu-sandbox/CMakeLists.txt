## Create the executable target
add_executable(satemu-sandbox
    src/main.cpp
)
add_executable(satemu::satemu-sandbox ALIAS satemu-sandbox)
set_target_properties(satemu-sandbox PROPERTIES
                      VERSION ${satemu_VERSION}
                      SOVERSION ${satemu_VERSION_MAJOR})
target_link_libraries(satemu-sandbox PRIVATE satemu::satemu-frontend-base)
target_compile_features(satemu-sandbox PUBLIC cxx_std_20)

## Add dependencies
target_link_libraries(satemu-sandbox PRIVATE fmt)

cmrk_copy_runtime_dlls(satemu-sandbox)

## Configure Visual Studio solution
if (MSVC)
    vs_set_filters(TARGET satemu-sandbox)
    set_target_properties(satemu-sandbox PROPERTIES FOLDER "satemu")
endif ()

## Include the install rules if requested
if (satemu_INCLUDE_PACKAGING)
    add_subdirectory(packaging)
endif ()
