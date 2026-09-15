add_executable(nyx_test main.c)

configure_file(nyxtest.conf ${CMAKE_CURRENT_BINARY_DIR}/conf/nyxtest.conf COPYONLY )
configure_file(nyxtest.conf ${CMAKE_CURRENT_BINARY_DIR}/logs/nyxtest.log COPYONLY )

target_link_libraries(nyx_test ${PROJECT_NAME} dl pthread crypt)