file(GLOB SOURCES "OS*.c")

#add_definitions(-DOS_DEBUG_ENABLE)
add_compile_options( -O3 )

add_library(${PROJECT_NAME} STATIC ${SOURCES} )
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)