cmake_minimum_required(VERSION 3.28)
project(vermilion VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(vermilion_warnings INTERFACE)
target_compile_options(vermilion_warnings INTERFACE -Wall -Wextra -Werror -Wno-cast-function-type)

add_library(vermilion_noexc INTERFACE)
target_compile_options(vermilion_noexc INTERFACE -fno-exceptions)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)
pkg_check_modules(GTKSOURCEVIEW5 REQUIRED IMPORTED_TARGET gtksourceview-5)
pkg_check_modules(JSONGLIB REQUIRED IMPORTED_TARGET json-glib-1.0)
pkg_check_modules(LIBSOUP3 REQUIRED IMPORTED_TARGET libsoup-3.0)
pkg_check_modules(LIBSECRET REQUIRED IMPORTED_TARGET libsecret-1)
pkg_check_modules(VTE REQUIRED IMPORTED_TARGET vte-2.91-gtk4)

find_package(GTest REQUIRED)
enable_testing()

add_subdirectory(src/core)
add_subdirectory(src/agent)
add_subdirectory(src/ui)
add_subdirectory(tests)
