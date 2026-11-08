add_executable(vermilion_ctxmon_tests format_test.cpp log_tokens_test.cpp timing_test.cpp)
target_compile_definitions(vermilion_ctxmon_tests PRIVATE
    CTXMON_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_link_libraries(vermilion_ctxmon_tests PRIVATE
    GTest::gtest_main
    vermilion_ctxmon_core
    vermilion_warnings
    PkgConfig::JSONGLIB
    PkgConfig::GLIB2)
gtest_discover_tests(vermilion_ctxmon_tests)
