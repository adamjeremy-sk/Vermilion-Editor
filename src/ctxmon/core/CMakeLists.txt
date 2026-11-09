add_library(vermilion_ctxmon_core STATIC format.cpp log_tokens.cpp timing.cpp server_api.cpp)
target_include_directories(vermilion_ctxmon_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(vermilion_ctxmon_core PRIVATE vermilion_warnings vermilion_noexc PkgConfig::GLIB2 PkgConfig::JSONGLIB)
