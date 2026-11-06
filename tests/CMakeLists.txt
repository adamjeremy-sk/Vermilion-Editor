add_executable(vermilion_tests core_version_test.cpp agent_info_test.cpp)
target_link_libraries(vermilion_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_agent
    vermilion_warnings)
gtest_discover_tests(vermilion_tests)
