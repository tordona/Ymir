# For developers:
#
# Try to keep vendored dependencies at a minimum to minimize bloat and make it easier for other
# developers to compile the code. Ideally they should be included as submodules under this folder.
#
# find_package(...) can be used with any dependencies that misbehave (e.g. libraries that always
# include tests, or require tools that are unnecessary to consume it, like Python). Consider using
# a simpler alternative library, creating a custom CMakeLists.txt or manually building the target
# here from the source files.
# If you do add a library with find_package(...), list in the README so that other developers can
# provide them in the system to build this project.
#
# --------------------------------------------------------------------------------------------------

## Helper function to silence messages from noisy subprojects
function(add_subdirectory_quiet DIR)
    function(message)
    endfunction()
    add_subdirectory(${DIR} EXCLUDE_FROM_ALL)
endfunction()

message(STATUS "Adding vendored libraries")

# {fmt} - https://github.com/fmtlib/fmt
message(VERBOSE "==> {fmt}")
add_subdirectory(fmt EXCLUDE_FROM_ALL)

# mio - https://github.com/mandreyel/mio
# Notes:
# - Using StrikerX3's fork instead
#   - This fork implements the features in this PR: https://github.com/mandreyel/mio/pull/78
#   - It also stops the library from including CTest unconditionally, which is not done in that PR
message(VERBOSE "==> mio")
add_subdirectory(mio EXCLUDE_FROM_ALL)

# SDL - https://github.com/libsdl-org/SDL
message(VERBOSE "==> SDL")
set(SDL_DISABLE_UNINSTALL ON)
set(SDL_TEST_LIBRARY OFF)
set(SDL_TESTS OFF)
set(SDL_EXAMPLES OFF)
if(BUILD_SHARED_LIBS)
    set(SDL_SHARED ON)
    set(SDL_STATIC OFF)
else()
    set(SDL_SHARED OFF)
    set(SDL_STATIC ON)
endif()
add_subdirectory_quiet(SDL)

# cxxopts - https://github.com/jarro2783/cxxopts
message(VERBOSE "==> cxxopts")
add_subdirectory(cxxopts EXCLUDE_FROM_ALL)

# concurrentqueue - https://github.com/cameron314/concurrentqueue
message(VERBOSE "==> concurrentqueue")
add_subdirectory(concurrentqueue EXCLUDE_FROM_ALL)

# dear ImGui - https://github.com/ocornut/imgui
# docking branch
message(VERBOSE "==> dear ImGui")
add_subdirectory(imgui EXCLUDE_FROM_ALL)

# toml++ - https://github.com/marzer/tomlplusplus
message(VERBOSE "==> toml++")
add_subdirectory(tomlplusplus EXCLUDE_FROM_ALL)

# --------------------------------------------------------------------------------------------------
# Test dependencies

if(satemu_ENABLE_TESTS)
    # Catch2 - https://github.com/catchorg/Catch2
    message(VERBOSE "==> Catch2")
    add_subdirectory(Catch2 EXCLUDE_FROM_ALL)
endif()

# --------------------------------------------------------------------------------------------------

# Place vendored library projects under a Vendored folder in Visual Studio solutions
if(MSVC)
    set_target_properties(fmt PROPERTIES FOLDER Vendored)
    if(BUILD_SHARED_LIBS)
        set_target_properties(SDL3-shared PROPERTIES FOLDER Vendored)
    else()
        set_target_properties(SDL3-static PROPERTIES FOLDER Vendored)
    endif()
    set_target_properties(SDL_uclibc PROPERTIES FOLDER Vendored)
    set_target_properties(Catch2 PROPERTIES FOLDER Vendored)
    set_target_properties(Catch2WithMain PROPERTIES FOLDER Vendored)
    set_target_properties(imgui PROPERTIES FOLDER Vendored)
endif()

message(STATUS "Done adding vendored libraries")
