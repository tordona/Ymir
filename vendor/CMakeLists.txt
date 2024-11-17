# Policies for vendored libraries to be included as subprojects:
#
# 0. Be a CMake project
#   - Projects that don't use CMake are allowed if making a custom compliant CMakeLists.txt for them
#     takes a couple of minutes at most
#   - Simple header-only projects are usually safe
# 1. Be prepared to be included as a subproject
#    - Must automatically disable all non-essential stuff and build a library only
#      - Do not include CTest, CPack, etc. when used as a submodule unless requested
#    - Must allow selecting between shared/static builds by either exposing different targets or
#      (ideally) configuring as a shared or static library
#    - Must not force the parent project to set global variables that may impact other projects
#      - Variables with names specific to the subproject are fine
#    - Must not introduce unnecessary dependencies or projects into the workspace
#      - Ideally, one dependency = one project (or zero, if header-only)
# 2. Compile cleanly and easily on all target systems
# 3. Compile cleanly with all supported compilers
# 4. Minimize external dependencies -- none is best
# 5. Do not pollute the solution/workspace when included as subprojects
#   - We only require a SHARED, STATIC, OBJECT or INTERFACE target and nothing else
#   - Executables, tests, custom commands, dummy projects, etc. are forbidden
#     - Especially true if the test projects, and executables require tools and/or features that the
#       library itself doesn't (e.g. requiring Python for tests only)
# 6. Keep CMake messages to a minimum
#   - One or two lines telling the version or configuration is fine
#   - A whole screen of useless information is not
#     - If there is no way to silence or tone down the messages, use add_subdirectory_quiet
# 7. NO CMAKE WARNINGS OF ANY KIND -- policies, deprecation, old cmake_minimum_required version, ...
#
# This list is non-exhaustive, but it boils down to: be a good citizen, provide a single STATIC,
# SHARED, OBJECT or INTERFACE target, and be quiet.
#
# --------------------------------------------------------------------------------------------------
#
# For developers:
#
# Try to keep vendored dependencies at a minimum to minimize bloat and make it easier for other
# developers to compile the code.
#
# find_package(...) can be used with any dependencies that do not adhere by these policies. However,
# consider using a simpler alternative library or coding your own solution if it is simple enough.
# If you do add a library in this manner, list it below and in the README so that other developers
# can provide them to build the library.
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

# --------------------------------------------------------------------------------------------------

# Place vendored library projects under a Vendored folder in Visual Studio solutions
if(MSVC)
    set_target_properties(fmt PROPERTIES FOLDER Vendored)
endif()
