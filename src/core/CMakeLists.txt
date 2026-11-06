add_library(vermilion_core STATIC version.cpp)
target_include_directories(vermilion_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(vermilion_core PUBLIC vermilion_warnings vermilion_noexc)
