add_subdirectory(gtkx)

add_executable(vermilion main.cpp)
target_include_directories(vermilion PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(vermilion PRIVATE
    PkgConfig::GTK4
    vermilion_core
    vermilion_agent
    vermilion_gtkx
    vermilion_warnings
    vermilion_noexc)
