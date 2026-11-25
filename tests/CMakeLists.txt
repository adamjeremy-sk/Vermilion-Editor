add_executable(vermilion_tests core_version_test.cpp agent_info_test.cpp result_test.cpp redact_test.cpp log_test.cpp document_list_test.cpp text_format_test.cpp text_scan_test.cpp document_list_files_test.cpp open_messages_test.cpp text_format_ids_test.cpp document_list_save_test.cpp save_messages_test.cpp manual_spans_test.cpp manual_page_test.cpp page_history_test.cpp manual_files_test.cpp recovery_file_test.cpp recovery_file_reject_test.cpp recovery_schedule_test.cpp recovery_store_test.cpp recovery_store_files_test.cpp recovery_tracker_test.cpp recovery_tracker_rules_test.cpp recovery_tracker_events_test.cpp recovery_tracker_adopt_test.cpp recovery_offer_test.cpp recovery_question_test.cpp baseline_test.cpp document_list_deleted_test.cpp recovery_answers_test.cpp recovery_blank_test.cpp search_options_test.cpp search_replacement_test.cpp count_text_test.cpp goto_line_test.cpp goto_column_test.cpp search_messages_test.cpp held_events_test.cpp file_hash_test.cpp text_scan_hash_test.cpp settings_file_test.cpp external_change_test.cpp external_messages_test.cpp document_list_encoding_test.cpp recovery_tracker_deleted_test.cpp reload_format_test.cpp external_look_test.cpp external_reload_test.cpp open_arguments_test.cpp open_request_test.cpp open_queue_test.cpp create_questions_test.cpp settings_values_test.cpp colour_mode_test.cpp editor_settings_test.cpp editor_look_test.cpp settings_io_test.cpp manual_table_test.cpp manual_text_test.cpp manual_text_files_test.cpp page_history_forward_test.cpp manual_outline_test.cpp manual_outline_files_test.cpp manual_search_test.cpp manual_search_files_test.cpp manual_results_test.cpp manual_results_files_test.cpp)
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

# BackgroundScope on a real GIO thread pool and main loop, no window and no display (GTK4 is
# linked for the GIO headers only; no widget is created).
add_executable(vermilion_background_task_tests background_task_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/background_task.cpp)
target_link_libraries(vermilion_background_task_tests PRIVATE
    GTest::gtest_main
    vermilion_warnings
    PkgConfig::GTK4)
target_include_directories(vermilion_background_task_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
gtest_discover_tests(vermilion_background_task_tests)

# ManualLibrary: the manual made from text and from the program's resources, and the library that
# is prepared on a worker thread; no window and no display (GTK4 is linked for the GIO headers
# only; no widget is created).
add_executable(vermilion_manual_library_tests manual_library_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/manual_library.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/background_task.cpp)
target_link_libraries(vermilion_manual_library_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_resources
    vermilion_warnings
    PkgConfig::GTK4)
target_include_directories(vermilion_manual_library_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(vermilion_manual_library_tests PRIVATE
    VERMILION_MANUAL_DIR="${CMAKE_SOURCE_DIR}/docs/manual")
gtest_discover_tests(vermilion_manual_library_tests)

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

# The dark editor scheme and the dark palette sheet (a separate program because the scheme code
# keeps its folder in a static; each program sets its own XDG_CACHE_HOME).
add_executable(vermilion_editor_scheme_dark_tests editor_scheme_dark_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/editor_scheme.cpp)
target_link_libraries(vermilion_editor_scheme_dark_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_gtkx
    vermilion_resources
    vermilion_warnings
    PkgConfig::GTKSOURCEVIEW5)
gtest_discover_tests(vermilion_editor_scheme_dark_tests)

# Style on a real display (the tests skip without one): the palettes, the prefer-dark flag, the
# parsing of the stylesheets.
add_executable(vermilion_style_tests style_test.cpp ${CMAKE_SOURCE_DIR}/src/ui/style.cpp)
target_link_libraries(vermilion_style_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_gtkx
    vermilion_resources
    vermilion_warnings)
gtest_discover_tests(vermilion_style_tests)

# The appearance controller with a recording host and a hand-driven timer: no GTK at all.
add_executable(vermilion_appearance_tests appearance_controller_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/appearance_controller.cpp)
target_link_libraries(vermilion_appearance_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_warnings
    PkgConfig::GLIB2)
gtest_discover_tests(vermilion_appearance_tests)

# font_css: Pango's font parser, no display and no main loop.
add_executable(vermilion_font_css_tests font_css_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/font_css.cpp)
target_include_directories(vermilion_font_css_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(vermilion_font_css_tests PRIVATE
    GTest::gtest_main
    vermilion_warnings
    PkgConfig::GTK4)
gtest_discover_tests(vermilion_font_css_tests)

# FontStyle on a real display (the tests skip without one): the rule reaches a widget.
add_executable(vermilion_font_style_tests font_style_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/font_style.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/font_css.cpp)
target_link_libraries(vermilion_font_style_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_gtkx
    vermilion_warnings)
gtest_discover_tests(vermilion_font_style_tests)

# The look of the editor pages on a real display (the tests skip without one): EditorPage and
# DocumentTabs with the sources they need.
add_executable(vermilion_page_look_tests editor_page_look_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/editor_page.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/editor_page_look.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/editor_scheme.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/document_tabs.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/document_tabs_look.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/document_tabs_reload.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/document_loader.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/document_saver.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/change_banner.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/tab_label.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/file_inspector.cpp)
target_link_libraries(vermilion_page_look_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_gtkx
    vermilion_resources
    vermilion_warnings
    PkgConfig::GTKSOURCEVIEW5)
gtest_discover_tests(vermilion_page_look_tests)

# The host of the appearance controller on real parts (style, font, tabs; the tests skip without a
# display).
add_executable(vermilion_appearance_host_tests appearance_host_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/appearance_host.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/appearance_controller.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/style.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/font_style.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/font_css.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/editor_page.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/editor_page_look.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/editor_scheme.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/document_tabs.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/document_tabs_look.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/document_tabs_reload.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/document_loader.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/document_saver.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/change_banner.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/tab_label.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/file_inspector.cpp)
target_link_libraries(vermilion_appearance_host_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_gtkx
    vermilion_resources
    vermilion_warnings
    PkgConfig::GTKSOURCEVIEW5)
gtest_discover_tests(vermilion_appearance_host_tests)

# SystemTheme: the portal's value and the theme name on a real display (the tests skip without one).
add_executable(vermilion_system_theme_tests system_theme_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/system_theme.cpp)
target_link_libraries(vermilion_system_theme_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_gtkx
    vermilion_warnings
    PkgConfig::GTK4)
gtest_discover_tests(vermilion_system_theme_tests)

# The Settings window shell on a real display (the tests skip without one): a fake page stands for
# the real ones.
add_executable(vermilion_settings_window_tests settings_window_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/settings_window.cpp)
target_link_libraries(vermilion_settings_window_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_gtkx
    vermilion_warnings
    PkgConfig::GTK4)
gtest_discover_tests(vermilion_settings_window_tests)

# The Appearance tab on a real display (the tests skip without one).
add_executable(vermilion_settings_page_appearance_tests settings_page_appearance_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/settings_page_appearance.cpp)
target_link_libraries(vermilion_settings_page_appearance_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_gtkx
    vermilion_warnings
    PkgConfig::GTK4)
gtest_discover_tests(vermilion_settings_page_appearance_tests)

# The Editor and Files tabs on a real display (the tests skip without one).
add_executable(vermilion_settings_page_editor_tests settings_page_editor_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/settings_page_editor.cpp)
target_link_libraries(vermilion_settings_page_editor_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_gtkx
    vermilion_warnings
    PkgConfig::GTK4)
gtest_discover_tests(vermilion_settings_page_editor_tests)

add_executable(vermilion_settings_page_files_tests settings_page_files_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/settings_page_files.cpp)
target_link_libraries(vermilion_settings_page_files_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_gtkx
    vermilion_warnings
    PkgConfig::GTK4)
gtest_discover_tests(vermilion_settings_page_files_tests)

# The shortcut table of the main window (no display needed).
add_executable(vermilion_window_shortcuts_tests window_shortcuts_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/window_shortcuts.cpp)
target_link_libraries(vermilion_window_shortcuts_tests PRIVATE
    GTest::gtest_main
    vermilion_gtkx
    vermilion_warnings
    PkgConfig::GTK4)
gtest_discover_tests(vermilion_window_shortcuts_tests)

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

# The Help page view on a real display (the tests skip without one).
add_executable(vermilion_help_page_view_tests help_page_view_test.cpp
    help_page_view_link_test.cpp help_page_view_zoom_test.cpp help_page_view_margin_test.cpp
    help_page_view_colour_test.cpp help_page_view_layout_test.cpp help_page_view_search_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/help_page_view.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/help_page_view_zoom.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/help_page_view_layout.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/help_page_view_colours.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/help_page_view_search.cpp)
target_link_libraries(vermilion_help_page_view_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_gtkx
    vermilion_warnings
    PkgConfig::GTK4)
gtest_discover_tests(vermilion_help_page_view_tests)

# The Contents tree of the Help window on a real display (the tests skip without one).
add_executable(vermilion_help_contents_tests help_contents_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/help_contents.cpp)
target_link_libraries(vermilion_help_contents_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_gtkx
    vermilion_warnings
    PkgConfig::GTK4)
gtest_discover_tests(vermilion_help_contents_tests)

# The Help window on a real display (the tests skip without one).
add_executable(vermilion_help_window_tests help_window_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/help_window.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/help_contents.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/manual_library.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/background_task.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/style.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/help_page_view.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/help_page_view_zoom.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/help_page_view_layout.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/help_page_view_colours.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/help_page_view_search.cpp)
target_link_libraries(vermilion_help_window_tests PRIVATE
    GTest::gtest_main
    vermilion_core
    vermilion_gtkx
    vermilion_resources
    vermilion_warnings
    PkgConfig::GTK4)
target_compile_definitions(vermilion_help_window_tests PRIVATE
    VERMILION_MANUAL_DIR="${CMAKE_SOURCE_DIR}/docs/manual")
gtest_discover_tests(vermilion_help_window_tests)

add_subdirectory(ctxmon)
