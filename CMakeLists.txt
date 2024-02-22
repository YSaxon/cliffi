cmake_minimum_required(VERSION 3.10)
project(ffitool VERSION 1.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 99)

# Find libffi
find_package(PkgConfig REQUIRED)
pkg_search_module(LIBFFI REQUIRED libffi)

# Include directories
include_directories(${LIBFFI_INCLUDE_DIRS})

# Source files
set(SOURCES
    main.c
    argparser.c
    types_and_utils.c
    invoke_handler.c
    library_path_resolver.c
    return_formatter.c
)

# Target executable
add_executable(ffitool ${SOURCES})
target_link_libraries(ffitool ${LIBFFI_LIBRARIES})
target_include_directories(ffitool PRIVATE ${LIBFFI_INCLUDE_DIRS})

# Compile ffitool_testlib.c into a shared library
add_library(ffitooltest SHARED ffitool_testlib.c)

# Set the output name of the library to match the platform conventions
set_target_properties(ffitooltest PROPERTIES OUTPUT_NAME "ffitool_test")
