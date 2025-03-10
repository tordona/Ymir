# For developers:
#
# Try to keep vendored dependencies at a minimum to minimize bloat and make it easier for other
# developers to compile the code. Ideally they should be included as submodules under the vendor
# directory.
#
# find_package(...) can be used with any dependencies that misbehave (e.g. libraries that always
# include tests, or require tools that are unnecessary to consume it, like Python). Consider using
# a simpler alternative library or coding your own solution if it is simple enough.
# If you do add a library in this manner, list it below and in the README so that other developers
# can provide them in the system to build this project.
#
# --------------------------------------------------------------------------------------------------
#
# Vendored libraries required but not included as a submodule:
#   (None so far. If any are included, list their name and URL here, sorted alphabetically by name)
#
# --------------------------------------------------------------------------------------------------

## Helper function to silence messages from noisy subprojects
function(add_subdirectory_quiet DIR)
    function(message)
    endfunction()
    add_subdirectory(${DIR})
endfunction()

message(STATUS "Adding vendored libraries")

# {fmt} - https://github.com/fmtlib/fmt
message(VERBOSE "==> {fmt}")
add_subdirectory(fmt)

# mio - https://github.com/mandreyel/mio
# Notes:
# - Using StrikerX3's fork instead
#   - This fork implements the features in this PR: https://github.com/mandreyel/mio/pull/78
#   - It also stops the library from including CTest unconditionally, which is not done in that PR
message(VERBOSE "==> mio")
add_subdirectory(mio)

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
add_subdirectory(cxxopts)

# concurrentqueue - https://github.com/cameron314/concurrentqueue
message(VERBOSE "=>> concurrentqueue")
add_subdirectory(concurrentqueue)

if(satemu_ENABLE_TESTS)
    # Catch2 - https://github.com/catchorg/Catch2
    message(VERBOSE "==> Catch2")
    add_subdirectory(Catch2)
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
endif()
