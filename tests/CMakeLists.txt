add_executable(vermilion_tests core_version_test.cpp agent_info_test.cpp result_test.cpp redact_test.cpp log_test.cpp document_list_test.cpp text_format_test.cpp text_scan_test.cpp document_list_files_test.cpp open_messages_test.cpp text_format_ids_test.cpp document_list_save_test.cpp save_messages_test.cpp manual_spans_test.cpp manual_page_test.cpp page_history_test.cpp manual_files_test.cpp recovery_file_test.cpp recovery_file_reject_test.cpp recovery_schedule_test.cpp recovery_store_test.cpp recovery_store_files_test.cpp recovery_tracker_test.cpp recovery_tracker_rules_test.cpp recovery_tracker_events_test.cpp recovery_tracker_adopt_test.cpp recovery_offer_test.cpp recovery_question_test.cpp recovered_base_test.cpp recovery_answers_test.cpp recovery_blank_test.cpp)
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

# One ctest test for the whole file: its checks share one run of about eleven seconds on the real
# clock (the schedule waits five seconds of quiet before it copies).
add_executable(vermilion_recovery_controller_tests recovery_controller_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/recovery_controller.cpp)
target_link_libraries(vermilion_recovery_controller_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_warnings
    PkgConfig::GLIB2)
add_test(NAME RecoveryController.Timeline COMMAND vermilion_recovery_controller_tests)
set_tests_properties(RecoveryController.Timeline PROPERTIES TIMEOUT 120)

# The controller taking over the id of a recovered document's copy: a few short tests on a real
# main loop, one of them about six seconds long.
add_executable(vermilion_recovery_adopt_tests recovery_controller_adopt_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/recovery_controller.cpp)
target_link_libraries(vermilion_recovery_adopt_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_warnings
    PkgConfig::GLIB2)
gtest_discover_tests(vermilion_recovery_adopt_tests)

# The start-up question's file helpers (a file's time, a copy read on a worker thread), on a real
# main loop. GTK4 is linked for the GIO headers and library only; no widget is created.
add_executable(vermilion_recovery_io_tests recovery_io_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/recovery_io.cpp)
target_link_libraries(vermilion_recovery_io_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_warnings
    PkgConfig::GTK4)
gtest_discover_tests(vermilion_recovery_io_tests)

# The start-up question's walk, with a fake window, the real store and a real main loop. GTK4 is
# linked for the GIO headers (recovery_io.cpp) only; no widget is created.
add_executable(vermilion_recovery_prompt_tests recovery_prompt_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/recovery_prompt.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/recovery_io.cpp)
target_link_libraries(vermilion_recovery_prompt_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_warnings
    PkgConfig::GTK4)
gtest_discover_tests(vermilion_recovery_prompt_tests)

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
