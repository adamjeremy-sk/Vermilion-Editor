add_executable(vermilion-spike-largefile largefile_spike.cpp)
target_include_directories(vermilion-spike-largefile PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(vermilion-spike-largefile PRIVATE
    PkgConfig::GTK4
    PkgConfig::GTKSOURCEVIEW5
    vermilion_gtkx
    vermilion_warnings
    vermilion_noexc)

add_executable(vermilion-spike-look look_spike.cpp look_style.cpp look_widgets.cpp look_portal.cpp)
target_include_directories(vermilion-spike-look PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(vermilion-spike-look PRIVATE
    VERMILION_SPIKE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(vermilion-spike-look PRIVATE
    PkgConfig::GTK4
    PkgConfig::GTKSOURCEVIEW5
    vermilion_gtkx
    vermilion_warnings
    vermilion_noexc)
