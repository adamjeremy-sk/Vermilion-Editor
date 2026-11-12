add_executable(vermilion_tests core_version_test.cpp agent_info_test.cpp result_test.cpp redact_test.cpp log_test.cpp document_list_test.cpp text_format_test.cpp text_scan_test.cpp document_list_files_test.cpp open_messages_test.cpp text_format_ids_test.cpp document_list_save_test.cpp save_messages_test.cpp manual_spans_test.cpp manual_page_test.cpp page_history_test.cpp manual_files_test.cpp)
target_link_libraries(vermilion_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_agent
    vermilion_warnings
    PkgConfig::GLIB2)
target_compile_definitions(vermilion_tests PRIVATE
    VERMILION_MANUAL_DIR="${CMAKE_SOURCE_DIR}/docs/manual")
gtest_discover_tests(vermilion_tests)

add_executable(vermilion_gtkx_tests gtkx_object_ptr_test.cpp gtkx_string_test.cpp gtkx_signal_test.cpp gtkx_async_guard_test.cpp)
target_link_libraries(vermilion_gtkx_tests PRIVATE
    GTest::gtest_main
    vermilion_gtkx
    vermilion_warnings)
gtest_discover_tests(vermilion_gtkx_tests)

add_executable(vermilion_ui_tests ui_resources_test.cpp manual_resources_test.cpp)
target_link_libraries(vermilion_ui_tests PRIVATE
    GTest::gtest_main
    vermilion_resources
    vermilion_warnings)
target_compile_definitions(vermilion_ui_tests PRIVATE
    VERMILION_MANUAL_DIR="${CMAKE_SOURCE_DIR}/docs/manual")
gtest_discover_tests(vermilion_ui_tests)
add_test(NAME SourceCheck.Repo COMMAND ${CMAKE_SOURCE_DIR}/tools/check_sources.sh)
add_test(NAME Format.Check COMMAND ${CMAKE_SOURCE_DIR}/tools/check_format.sh)
set_tests_properties(Format.Check PROPERTIES SKIP_RETURN_CODE 77)
add_subdirectory(ctxmon)
