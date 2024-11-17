## Defines the following macros on the specified target with the values coming from the same named variables:
## - ${PROJECT_NAME}_VERSION
## - ${PROJECT_NAME}_VERSION_MAJOR
## - ${PROJECT_NAME}_VERSION_MINOR
## - ${PROJECT_NAME}_VERSION_PATCH
macro(target_add_version_defines TARGET)
	target_compile_definitions(${TARGET} PUBLIC ${PROJECT_NAME}_VERSION="${${PROJECT_NAME}_VERSION}")
	target_compile_definitions(${TARGET} PUBLIC ${PROJECT_NAME}_VERSION_MAJOR=${${PROJECT_NAME}_VERSION_MAJOR}u)
	target_compile_definitions(${TARGET} PUBLIC ${PROJECT_NAME}_VERSION_MINOR=${${PROJECT_NAME}_VERSION_MINOR}u)
	target_compile_definitions(${TARGET} PUBLIC ${PROJECT_NAME}_VERSION_PATCH=${${PROJECT_NAME}_VERSION_PATCH}u)
endmacro()
