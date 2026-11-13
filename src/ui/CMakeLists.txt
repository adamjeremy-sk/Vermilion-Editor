add_subdirectory(gtkx)

# The application icon: every build renders the two SVG masters in res/vedt to PNG with
# rsvg-convert (a build tool only; the program reads PNG with GTK's own loader). The small badge
# serves 16 and 24 px, the page 32 px and up. The PNGs are written into the build folder in the
# icon theme layout <n>x<n>/apps/<application ID>.png and compiled into the resources below, under
# icons/, where GtkApplication looks for the application's own icons.
find_program(RSVG_CONVERT rsvg-convert)
if(NOT RSVG_CONVERT)
    message(FATAL_ERROR "rsvg-convert was not found. Install the librsvg2-bin package.")
endif()
set(VERMILION_ICON_NAME io.github.adamjeremy_sk.Vermilion)
set(VERMILION_ICON_SMALL ${CMAKE_SOURCE_DIR}/res/vedt/vedticon.svg)
set(VERMILION_ICON_LARGE ${CMAKE_SOURCE_DIR}/res/vedt/vermilion-icon.svg)
set(VERMILION_ICON_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/rendered)
set(VERMILION_ICON_PNGS)
foreach(size 16 24 32 48 64 128 256)
    if(size LESS 32)
        set(master ${VERMILION_ICON_SMALL})
    else()
        set(master ${VERMILION_ICON_LARGE})
    endif()
    set(icon_dir ${VERMILION_ICON_BUILD_DIR}/icons/${size}x${size}/apps)
    set(icon_png ${icon_dir}/${VERMILION_ICON_NAME}.png)
    add_custom_command(
        OUTPUT ${icon_png}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${icon_dir}
        COMMAND ${RSVG_CONVERT} --width=${size} --height=${size} --format=png
                --output=${icon_png} ${master}
        DEPENDS ${master}
        VERBATIM)
    list(APPEND VERMILION_ICON_PNGS ${icon_png})
endforeach()

# The stylesheets, the menu bar and the icons are compiled into the program
# (glib-compile-resources ships with GLib); the rendered icons come from the second --sourcedir.
# --manual-register keeps the static library from being dropped by the linker.
find_program(GLIB_COMPILE_RESOURCES glib-compile-resources REQUIRED)
set(VERMILION_RESOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources)
set(VERMILION_RESOURCE_XML ${VERMILION_RESOURCE_DIR}/vermilion.gresource.xml)
set(VERMILION_RESOURCE_CPP ${CMAKE_CURRENT_BINARY_DIR}/vermilion_resources.cpp)
set(VERMILION_RESOURCE_H ${CMAKE_CURRENT_BINARY_DIR}/vermilion_resources.h)
add_custom_command(
    OUTPUT ${VERMILION_RESOURCE_CPP} ${VERMILION_RESOURCE_H}
    COMMAND ${GLIB_COMPILE_RESOURCES} --sourcedir=${VERMILION_RESOURCE_DIR}
            --sourcedir=${VERMILION_ICON_BUILD_DIR} --c-name=vermilion
            --manual-register --generate-source --target=${VERMILION_RESOURCE_CPP}
            ${VERMILION_RESOURCE_XML}
    COMMAND ${GLIB_COMPILE_RESOURCES} --sourcedir=${VERMILION_RESOURCE_DIR}
            --sourcedir=${VERMILION_ICON_BUILD_DIR} --c-name=vermilion
            --manual-register --generate-header --target=${VERMILION_RESOURCE_H}
            ${VERMILION_RESOURCE_XML}
    DEPENDS ${VERMILION_RESOURCE_XML}
            ${VERMILION_RESOURCE_DIR}/classic_light.css
            ${VERMILION_RESOURCE_DIR}/classic_structure.css
            ${VERMILION_RESOURCE_DIR}/menus.ui
            ${VERMILION_ICON_PNGS}
    VERBATIM)
# The user manual (Adam, 2026-11-11): every docs/manual/*.md is compiled into the program as a
# second resource, under /io/github/adamjeremy_sk/Vermilion/manual/, so a new topic needs no code
# change. The file list is written at configure time; CONFIGURE_DEPENDS makes the build run CMake
# again when a topic is added or removed. The tests read the same folder (VERMILION_MANUAL_DIR).
file(GLOB VERMILION_MANUAL_FILES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/docs/manual/*.md)
list(SORT VERMILION_MANUAL_FILES)
set(VERMILION_MANUAL_ENTRIES "")
foreach(topic IN LISTS VERMILION_MANUAL_FILES)
    get_filename_component(topic_name ${topic} NAME)
    string(APPEND VERMILION_MANUAL_ENTRIES "    <file>${topic_name}</file>\n")
endforeach()
set(VERMILION_MANUAL_XML ${CMAKE_CURRENT_BINARY_DIR}/manual.gresource.xml)
file(CONFIGURE OUTPUT ${VERMILION_MANUAL_XML} CONTENT [=[<?xml version="1.0" encoding="UTF-8"?>
<gresources>
  <gresource prefix="/io/github/adamjeremy_sk/Vermilion/manual">
@VERMILION_MANUAL_ENTRIES@  </gresource>
</gresources>
]=] @ONLY)
set(VERMILION_MANUAL_CPP ${CMAKE_CURRENT_BINARY_DIR}/vermilion_manual.cpp)
set(VERMILION_MANUAL_H ${CMAKE_CURRENT_BINARY_DIR}/vermilion_manual.h)
add_custom_command(
    OUTPUT ${VERMILION_MANUAL_CPP} ${VERMILION_MANUAL_H}
    COMMAND ${GLIB_COMPILE_RESOURCES} --sourcedir=${CMAKE_SOURCE_DIR}/docs/manual
            --c-name=vermilion_manual --manual-register --generate-source
            --target=${VERMILION_MANUAL_CPP} ${VERMILION_MANUAL_XML}
    COMMAND ${GLIB_COMPILE_RESOURCES} --sourcedir=${CMAKE_SOURCE_DIR}/docs/manual
            --c-name=vermilion_manual --manual-register --generate-header
            --target=${VERMILION_MANUAL_H} ${VERMILION_MANUAL_XML}
    DEPENDS ${VERMILION_MANUAL_XML} ${VERMILION_MANUAL_FILES}
    VERBATIM)
add_library(vermilion_resources STATIC ${VERMILION_RESOURCE_CPP} ${VERMILION_RESOURCE_H}
                                      ${VERMILION_MANUAL_CPP} ${VERMILION_MANUAL_H})
target_include_directories(vermilion_resources PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(vermilion_resources
    PUBLIC PkgConfig::GTK4
    PRIVATE vermilion_warnings vermilion_noexc)

add_executable(vermilion main.cpp main_window.cpp document_tabs.cpp tab_label.cpp status_bar.cpp style.cpp close_dialog.cpp file_probe.cpp document_loader.cpp editor_page.cpp file_opener.cpp document_saver.cpp save_controller.cpp format_commands.cpp window_actions.cpp help_window.cpp recovery_controller.cpp recovery_io.cpp recovery_prompt.cpp main_window_recovery.cpp)
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
