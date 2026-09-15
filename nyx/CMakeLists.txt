cmake_minimum_required(VERSION 3.10)

project(nyx)

include_directories( include )
add_compile_options(  -Wall -Wextra -Werror -Wno-format-truncation -Wno-stringop-truncation )

add_subdirectory(src)
add_subdirectory(tool)