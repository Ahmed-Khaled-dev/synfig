# fixme(ice0): should be exported by synfig
include_directories(${PROJECT_SOURCE_DIR}/src)

add_executable(test_synfig_angle angle.cpp)
target_link_libraries(test_synfig_angle PRIVATE synfig)
add_test(NAME test_synfig_angle COMMAND test_synfig_angle)

add_executable(test_synfig_benchmark benchmark.cpp)
target_link_libraries(test_synfig_benchmark PRIVATE synfig)
add_test(NAME test_synfig_benchmark COMMAND test_synfig_benchmark)

add_executable(test_synfig_bline bline.cpp)
target_link_libraries(test_synfig_bline PRIVATE synfig)
add_test(NAME test_synfig_bline COMMAND test_synfig_bline)

add_executable(test_synfig_bone bone.cpp)
target_link_libraries(test_synfig_bone PRIVATE synfig)
add_test(NAME test_synfig_bone COMMAND test_synfig_bone)

add_executable(test_synfig_clock clock.cpp)
target_link_libraries(test_synfig_clock PRIVATE synfig)
add_test(NAME test_synfig_clock COMMAND test_synfig_clock)

#add_executable(test_synfig_node node.cpp)
#target_link_libraries(test_synfig_node PRIVATE synfig)
#add_test(NAME test_synfig_node COMMAND test_synfig_node)

#add_executable(test_synfig_string string.cpp)
#target_link_libraries(test_synfig_string PRIVATE synfig)
#add_test(NAME test_synfig_string COMMAND test_synfig_string)