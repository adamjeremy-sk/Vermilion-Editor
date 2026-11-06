add_executable(vermilion_tests core_version_test.cpp agent_info_test.cpp)
target_link_libraries(vermilion_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_agent
    vermilion_warnings)
gtest_discover_tests(vermilion_tests)

add_executable(vermilion_gtkx_tests gtkx_object_ptr_test.cpp gtkx_string_test.cpp gtkx_signal_test.cpp)
target_link_libraries(vermilion_gtkx_tests PRIVATE
    GTest::gtest_main
    vermilion_gtkx
    vermilion_warnings)
gtest_discover_tests(vermilion_gtkx_tests)
