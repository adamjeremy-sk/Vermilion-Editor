add_library(vermilion_core STATIC version.cpp result.cpp redact.cpp log.cpp document_list.cpp text_format.cpp text_scan.cpp open_messages.cpp save_messages.cpp manual_spans.cpp manual_page.cpp page_history.cpp recovery_file.cpp recovery_schedule.cpp recovery_store.cpp recovery_tracker.cpp recovery_offer.cpp recovery_question.cpp baseline.cpp recovery_answers.cpp recovery_blank.cpp search_options.cpp search_replacement.cpp count_text.cpp goto_line.cpp goto_column.cpp search_messages.cpp file_hash.cpp settings_file.cpp external_change.cpp external_messages.cpp reload_format.cpp open_arguments.cpp open_queue.cpp create_questions.cpp colour_mode.cpp editor_settings.cpp settings_io.cpp cache_file.cpp manual_table.cpp manual_text.cpp)
target_include_directories(vermilion_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
# The vendored xxHash header is a system header, so the project's warnings do not apply to it. Only
# file_hash.cpp includes it, which is why the directory is PRIVATE.
target_include_directories(vermilion_core SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party/xxhash)
target_link_libraries(vermilion_core PRIVATE vermilion_warnings vermilion_noexc PkgConfig::GLIB2)
