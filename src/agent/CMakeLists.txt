add_library(vermilion_agent STATIC agent_info.cpp)
target_include_directories(vermilion_agent PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(vermilion_agent PUBLIC vermilion_core)
