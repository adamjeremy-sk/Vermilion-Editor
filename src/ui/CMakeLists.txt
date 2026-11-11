add_subdirectory(gtkx)

# The stylesheets and the menu bar are compiled into the program (glib-compile-resources ships
# with GLib). --manual-register keeps the static library from being dropped by the linker.
find_program(GLIB_COMPILE_RESOURCES glib-compile-resources REQUIRED)
set(VERMILION_RESOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources)
set(VERMILION_RESOURCE_XML ${VERMILION_RESOURCE_DIR}/vermilion.gresource.xml)
set(VERMILION_RESOURCE_CPP ${CMAKE_CURRENT_BINARY_DIR}/vermilion_resources.cpp)
set(VERMILION_RESOURCE_H ${CMAKE_CURRENT_BINARY_DIR}/vermilion_resources.h)
add_custom_command(
    OUTPUT ${VERMILION_RESOURCE_CPP} ${VERMILION_RESOURCE_H}
    COMMAND ${GLIB_COMPILE_RESOURCES} --sourcedir=${VERMILION_RESOURCE_DIR} --c-name=vermilion
            --manual-register --generate-source --target=${VERMILION_RESOURCE_CPP}
            ${VERMILION_RESOURCE_XML}
    COMMAND ${GLIB_COMPILE_RESOURCES} --sourcedir=${VERMILION_RESOURCE_DIR} --c-name=vermilion
            --manual-register --generate-header --target=${VERMILION_RESOURCE_H}
            ${VERMILION_RESOURCE_XML}
    DEPENDS ${VERMILION_RESOURCE_XML}
            ${VERMILION_RESOURCE_DIR}/classic_light.css
            ${VERMILION_RESOURCE_DIR}/classic_structure.css
            ${VERMILION_RESOURCE_DIR}/menus.ui
    VERBATIM)
add_library(vermilion_resources STATIC ${VERMILION_RESOURCE_CPP} ${VERMILION_RESOURCE_H})
target_include_directories(vermilion_resources PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(vermilion_resources
    PUBLIC PkgConfig::GTK4
    PRIVATE vermilion_warnings vermilion_noexc)

add_executable(vermilion main.cpp main_window.cpp document_tabs.cpp tab_label.cpp status_bar.cpp style.cpp close_dialog.cpp)
target_include_directories(vermilion PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(vermilion PRIVATE
    PkgConfig::GTK4
    PkgConfig::GTKSOURCEVIEW5
    vermilion_core
    vermilion_agent
    vermilion_gtkx
    vermilion_resources
    vermilion_warnings
    vermilion_noexc)
