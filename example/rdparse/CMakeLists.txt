file(GLOB SRC *.cc)
add_executable(rdparse ${SRC})
target_link_libraries(rdparse PRIVATE netknot_static)
set_target_properties(rdparse PROPERTIES CXX_STANDARD 17)
