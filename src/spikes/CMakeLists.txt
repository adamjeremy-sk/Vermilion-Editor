add_executable(vermilion-spike-largefile largefile_spike.cpp)
target_include_directories(vermilion-spike-largefile PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(vermilion-spike-largefile PRIVATE
    PkgConfig::GTK4
    PkgConfig::GTKSOURCEVIEW5
    vermilion_gtkx
    vermilion_warnings
    vermilion_noexc)
