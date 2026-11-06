add_library(vermilion_gtkx STATIC string.cpp signal.cpp)
target_include_directories(vermilion_gtkx PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(vermilion_gtkx
    PUBLIC PkgConfig::GTK4
    PRIVATE vermilion_warnings vermilion_noexc)
