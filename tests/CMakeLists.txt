add_executable(vermilion_tests core_version_test.cpp agent_info_test.cpp result_test.cpp redact_test.cpp log_test.cpp document_list_test.cpp text_format_test.cpp text_scan_test.cpp document_list_files_test.cpp open_messages_test.cpp text_format_ids_test.cpp document_list_save_test.cpp save_messages_test.cpp manual_spans_test.cpp manual_page_test.cpp page_history_test.cpp manual_files_test.cpp recovery_file_test.cpp recovery_file_reject_test.cpp recovery_schedule_test.cpp recovery_store_test.cpp recovery_store_files_test.cpp recovery_tracker_test.cpp recovery_tracker_rules_test.cpp recovery_tracker_events_test.cpp recovery_tracker_adopt_test.cpp recovery_offer_test.cpp recovery_question_test.cpp baseline_test.cpp document_list_deleted_test.cpp recovery_answers_test.cpp recovery_blank_test.cpp search_options_test.cpp search_replacement_test.cpp count_text_test.cpp goto_line_test.cpp goto_column_test.cpp search_messages_test.cpp held_events_test.cpp file_hash_test.cpp text_scan_hash_test.cpp settings_file_test.cpp external_change_test.cpp external_messages_test.cpp document_list_encoding_test.cpp recovery_tracker_deleted_test.cpp reload_format_test.cpp external_look_test.cpp external_reload_test.cpp open_arguments_test.cpp open_request_test.cpp open_queue_test.cpp create_questions_test.cpp)
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

# A document whose file is gone keeps its recovery copy: one test of about six seconds on a real
# main loop, and two short ones.
add_executable(vermilion_recovery_deleted_tests recovery_controller_deleted_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/recovery_controller.cpp)
target_link_libraries(vermilion_recovery_deleted_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_warnings
    PkgConfig::GLIB2)
gtest_discover_tests(vermilion_recovery_deleted_tests)

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

# The search session on a real GtkSourceBuffer with no window and no display (GtkSourceView 5 is
# linked for the buffer and the search context; no widget is created).
add_executable(vermilion_search_session_tests search_session_test.cpp
    search_session_replace_test.cpp
    search_session_run_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/search_session.cpp)
target_link_libraries(vermilion_search_session_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_gtkx
    vermilion_warnings
    PkgConfig::GTKSOURCEVIEW5)
gtest_discover_tests(vermilion_search_session_tests)
# The GLib debounce timer on a real main loop, no window and no display.
add_executable(vermilion_glib_timer_tests glib_timer_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/glib_timer.cpp)
target_link_libraries(vermilion_glib_timer_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_warnings
    PkgConfig::GLIB2)
gtest_discover_tests(vermilion_glib_timer_tests)

# inspect_file on a real GIO thread pool and main loop, no window and no display (GTK4 is linked
# for the GIO headers only; no widget is created).
add_executable(vermilion_file_inspector_tests file_inspector_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/file_inspector.cpp)
target_link_libraries(vermilion_file_inspector_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_warnings
    PkgConfig::GTK4)
gtest_discover_tests(vermilion_file_inspector_tests)

# FileWatcher on a real GIO file monitor and main loop, no window and no display (GTK4 is linked
# for the GIO headers only; no widget is created). The timer is the real GlibTimer in most tests
# and a recording fake in two.
add_executable(vermilion_file_watcher_tests file_watcher_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/file_watcher.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/glib_timer.cpp)
target_link_libraries(vermilion_file_watcher_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_gtkx
    vermilion_warnings
    PkgConfig::GTK4)
gtest_discover_tests(vermilion_file_watcher_tests)

# ExternalChangeController on real files, a real GIO thread pool and file monitor and a real main
# loop, with a recording fake host: no window and no display (GTK4 is linked for the GIO headers
# only; no widget is created).
add_executable(vermilion_external_change_controller_tests external_change_controller_test.cpp
    external_change_look_test.cpp
    external_change_reload_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/external_change_controller.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/external_change_look.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/external_change_reload.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/file_inspector.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/file_probe.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/file_watcher.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/glib_timer.cpp)
target_link_libraries(vermilion_external_change_controller_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_gtkx
    vermilion_warnings
    PkgConfig::GTK4)
gtest_discover_tests(vermilion_external_change_controller_tests)

# The search controller on real buffers with no window and no display (the libraries of the
# search session; no widget is created). The timer is a fake in the tests.
add_executable(vermilion_search_controller_tests search_controller_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/search_controller.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/search_session.cpp)
target_link_libraries(vermilion_search_controller_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_gtkx
    vermilion_warnings
    PkgConfig::GTKSOURCEVIEW5)
gtest_discover_tests(vermilion_search_controller_tests)

# The replace controller on real buffers with no window and no display (the libraries of the
# search session; no widget is created). The timer never fires: the tests apply the text.
add_executable(vermilion_replace_controller_tests replace_controller_test.cpp
    replace_job_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/replace_controller.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/replace_controller_job.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/search_controller.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/search_session.cpp)
target_link_libraries(vermilion_replace_controller_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_gtkx
    vermilion_warnings
    PkgConfig::GTKSOURCEVIEW5)
gtest_discover_tests(vermilion_replace_controller_tests)

# Go to Line on real buffers with no window and no display (the libraries of the search session;
# no widget is created).
add_executable(vermilion_line_jump_tests line_jump_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/line_jump.cpp)
target_link_libraries(vermilion_line_jump_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_gtkx
    vermilion_warnings
    PkgConfig::GTKSOURCEVIEW5)
gtest_discover_tests(vermilion_line_jump_tests)

# The editor's style scheme, found in the program's resources through GtkSourceView's scheme
# manager (no window and no display).
add_executable(vermilion_editor_scheme_tests editor_scheme_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/editor_scheme.cpp)
target_link_libraries(vermilion_editor_scheme_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_gtkx
    vermilion_resources
    vermilion_warnings
    PkgConfig::GTKSOURCEVIEW5)
gtest_discover_tests(vermilion_editor_scheme_tests)

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
