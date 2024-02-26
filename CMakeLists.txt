cmake_minimum_required(VERSION 3.10)
project(ffitool VERSION 1.0 LANGUAGES C)

enable_testing()

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
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Target executable
add_executable(ffitool ${SOURCES})
target_link_libraries(ffitool ${LIBFFI_LIBRARIES})
target_include_directories(ffitool PRIVATE ${LIBFFI_INCLUDE_DIRS})

# Compile ffitool_testlib.c into a shared library
add_library(ffitooltest SHARED ffitool_testlib.c)

# Set the output name of the library to match the platform conventions
set_target_properties(ffitooltest PROPERTIES OUTPUT_NAME "ffitool_test")
# Set the output directory for executables and libraries
set_target_properties(ffitool PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
set_target_properties(ffitooltest PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

#===============================================================================
add_test(NAME TestAddFunction
         COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} i add 1 2)
set_tests_properties(TestAddFunction PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 3")

add_test(NAME TestMultiply
         COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} d multiply 1.5d -d 2)
set_tests_properties(TestMultiply PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 3.0")

add_test(NAME TestConcat
         COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} s concat -s "test" "abc 123")
set_tests_properties(TestConcat PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"testabc 123\"")

add_test(NAME TestGetMessage
         COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} s get_message)
set_tests_properties(TestGetMessage PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"Hello, ffitool!\"")