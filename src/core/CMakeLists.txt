add_library(vermilion_core STATIC version.cpp result.cpp redact.cpp log.cpp document_list.cpp text_format.cpp text_scan.cpp open_messages.cpp save_messages.cpp manual_spans.cpp manual_page.cpp page_history.cpp)
target_include_directories(vermilion_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(vermilion_core PRIVATE vermilion_warnings vermilion_noexc PkgConfig::GLIB2)
