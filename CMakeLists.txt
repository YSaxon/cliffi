cmake_minimum_required(VERSION 3.10)
project(cliffi VERSION 1.0 LANGUAGES C)

enable_testing()

# Set C standard
set(CMAKE_C_STANDARD 99)


set(USE_REPL_MODE ON)

if(APPLE)
    # Find Homebrew's LLVM installation directory
    execute_process(COMMAND brew --prefix llvm OUTPUT_VARIABLE HOMEBREW_LLVM_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)

    # Check if the directory exists
    find_program(LLVM_CLANG_FOUND clang++ PATHS ${HOMEBREW_LLVM_DIR}/bin NO_DEFAULT_PATH)

    if(LLVM_CLANG_FOUND)
        # Set C and C++ compilers
        set(CMAKE_C_COMPILER "${HOMEBREW_LLVM_DIR}/bin/clang")
        set(CMAKE_CXX_COMPILER "${HOMEBREW_LLVM_DIR}/bin/clang++")
        message(STATUS "Using Homebrew's LLVM Clang: ${CMAKE_C_COMPILER}")
    else()
        message(STATUS "Homebrew's LLVM is not installed, using default compiler")
    endif()
endif()

# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()


#if in debug mode enable coverage
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    endif()
endif()

option(ENABLE_SANITIZERS "Build with sanitizers enabled" OFF)
if(ENABLE_SANITIZERS)
    include(CheckCCompilerFlag)
    CHECK_C_COMPILER_FLAG("-fsanitize=address" COMPILER_SUPPORTS_ASAN)
    if(COMPILER_SUPPORTS_ASAN)
        message(STATUS "Building with AddressSanitizer")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
    endif()
    CHECK_C_COMPILER_FLAG("-fsanitize=undefined" COMPILER_SUPPORTS_UBSAN)
    if(COMPILER_SUPPORTS_UBSAN)
        message(STATUS "Building with undefined behavior sanitizer")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=undefined")
    endif()
endif()


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
library_manager.c
var_map.c
tokenize.c
parse_address.c
shims.c
exception_handling.c
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Target executable
add_executable(cliffi ${SOURCES})

if(NOT DEFINED USE_FIND_PACKAGE)

find_package(PkgConfig REQUIRED)

pkg_search_module(LIBFFI libffi REQUIRED)
pkg_search_module(READLINE readline REQUIRED)


target_link_libraries(cliffi PRIVATE
${LIBFFI_LIBRARIES}
${READLINE_LIBRARIES}
)

endif()
# Append the project root directory to CMAKE_PREFIX_PATH
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/build")


if(DEFINED USE_FIND_PACKAGE)
set(libffi_DIR ${CMAKE_SOURCE_DIR}/build)
set(ENV{libffi_DIR} ${CMAKE_SOURCE_DIR}/build)
find_package(libffi REQUIRED)
target_link_libraries(cliffi PRIVATE libffi::libffi)
message(STATUS "libffi include dirs: ${libffi_INCLUDE_DIRS}")


if (NOT WIN32)
set(readline_DIR ${CMAKE_SOURCE_DIR}/build)
set(ENV{readline_DIR} ${CMAKE_SOURCE_DIR}/build)
find_package(readline REQUIRED)
target_link_libraries(cliffi PRIVATE readline::readline)
message(STATUS "readline include dirs: ${readline_INCLUDE_DIRS}")
message("attempting link readline::readline")
endif()
endif()

find_library(DL_LIBRARY NAMES dl)
if(DL_LIBRARY)
  target_link_libraries(cliffi PRIVATE ${DL_LIBRARY})
endif()


target_include_directories(cliffi PRIVATE
        ${LIBFFI_INCLUDE_DIRS}
        ${READLINE_INCLUDE_DIRS}
        )

# Compile cliffi_testlib.c into a shared library
add_library(cliffitest SHARED cliffi_testlib.c)

# Set the output name of the library to match the platform conventions
set_target_properties(cliffitest PROPERTIES OUTPUT_NAME "cliffi_test")

# Set the output directory for executables and libraries
set_target_properties(cliffi PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
set_target_properties(cliffitest PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

#set variable for testlib path
set(TESTLIB ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX})

#===============================================================================
add_test(NAME TestAddFunction
         COMMAND cliffi ${TESTLIB} i add 1 2)
set_tests_properties(TestAddFunction PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 3")

add_test(NAME TestMultiply
         COMMAND cliffi ${TESTLIB} d multiply 1.5d -d 2)
set_tests_properties(TestMultiply PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 3.0")

add_test(NAME TestConcat
         COMMAND cliffi ${TESTLIB} s concat -s "test" "abc 123")
set_tests_properties(TestConcat PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"?testabc 123\"?")

add_test(NAME TestGetMessage
         COMMAND cliffi ${TESTLIB} s get_message)
set_tests_properties(TestGetMessage PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"?Hello, cliffi!\"?")

add_test(NAME TestSumArray
         COMMAND cliffi ${TESTLIB} i sum_array -ai 1,2,3 -i 3)
set_tests_properties(TestSumArray PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 6")

add_test(NAME TestIncrementAtPointer
         COMMAND cliffi ${TESTLIB} i increment_at_pointer -pi 5)
set_tests_properties(TestIncrementAtPointer PROPERTIES PASS_REGULAR_EXPRESSION "Function returned:.* 6")

add_test(NAME TestReturnArray
         COMMAND cliffi ${TESTLIB} ai6 get_array_of_int 3 6)
set_tests_properties(TestReturnArray PROPERTIES PASS_REGULAR_EXPRESSION "Function returned:.*\{ 3, 3, 3, 3, 3, 3 \}")

add_test(NAME TestReturnArrayDynamicallySized
         COMMAND cliffi ${TESTLIB} ait2 get_array_of_int 3 6)
set_tests_properties(TestReturnArrayDynamicallySized PROPERTIES PASS_REGULAR_EXPRESSION "Function returned:.*\{ 3, 3, 3, 3, 3, 3 \}")

add_test(NAME TestIncrementAtPointerPointer
         COMMAND cliffi ${TESTLIB} i increment_at_pointer_pointer -ppi 5)
set_tests_properties(TestIncrementAtPointerPointer PROPERTIES PASS_REGULAR_EXPRESSION "Function returned:.* 6")

add_test(NAME TestReturnArrayDoubles
         COMMAND cliffi ${TESTLIB} adt1 get_array_of_doubles 5)
set_tests_properties(TestReturnArrayDoubles PROPERTIES PASS_REGULAR_EXPRESSION "Function returned:.*\{ 0.00*, 0.50*, 1.00*, 1.50*, 2.00* \}")

add_test(NAME TestConcatStringArray
         COMMAND cliffi ${TESTLIB} s concat_str_array -as aa,bb,cc,dd 4)
set_tests_properties(TestConcatStringArray PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"?aabbccdd\"?")

add_test(NAME TestImplicitStringArray
COMMAND cliffi ${TESTLIB} s concat_str_array aa,bb,cc,dd 4)
set_tests_properties(TestImplicitStringArray PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"?aabbccdd\"?")

add_test(NAME TestImplicitIntArray
COMMAND cliffi ${TESTLIB} i sum_array 1,2,3 3)
set_tests_properties(TestImplicitIntArray PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 6")

add_test(NAME TestExplicitStringTypeOverridesArray
COMMAND cliffi ${TESTLIB} s concat -s 1,2,3 -s aa,bb,cc)
set_tests_properties(TestExplicitStringTypeOverridesArray PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"?1,2,3aa,bb,cc\"?")

add_test(NAME TestStringInQuotesAllowedSpacesAndDashes
COMMAND cliffi ${TESTLIB} s concat -s "1,2,3 -s aa,bb,cc" -s abc)
set_tests_properties(TestStringInQuotesAllowedSpacesAndDashes PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"?1,2,3 -s aa,bb,ccabc\"?")

add_test(NAME TestSumWithHex
         COMMAND cliffi ${TESTLIB} i add -i 0x10 -i 0x10)
set_tests_properties(TestSumWithHex PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 32")

add_test(NAME TestSumImplicitNegativeNumberNotInterpretedAsFlag
         COMMAND cliffi ${TESTLIB} i add -4 -5)
set_tests_properties(TestSumImplicitNegativeNumberNotInterpretedAsFlag PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: -9")

add_test(NAME TestConcatWithHexBuffers # aC and ac buffers should both work as strings if parsed properly
         COMMAND cliffi ${TESTLIB} s concat -ac 0x3232656800 -aC 0x34353600)
set_tests_properties(TestConcatWithHexBuffers PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"?22eh456\"?")

add_test(NAME TestReturningBuffer # aC and ac buffers should both work to contain strings
         COMMAND cliffi ${TESTLIB} ac8 get_message)
set_tests_properties(TestReturningBuffer PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: .*Hello,")

add_test(NAME TestReturningBufferWithSize # aC and ac buffers should both work to contain strings
            COMMAND cliffi ${TESTLIB} act2 buffer_as_return -act2 a,b,c,d 4)
set_tests_properties(TestReturningBufferWithSize PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: .*a.*b.*c.*d.*")

add_test(NAME TestGetX #
            COMMAND cliffi ${TESTLIB} i get_x -S: 5 2.2 :S )
set_tests_properties(TestGetX PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 5")

add_test(NAME TestGetXByReference #
            COMMAND cliffi ${TESTLIB} i get_x_from_structpointer -pS: 5 2.2 :S )
set_tests_properties(TestGetXByReference PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 5")

add_test(NAME ModifyPoint #
            COMMAND cliffi ${TESTLIB} v modify_point -pS: 5 2.2 :S 1 .1 )
set_tests_properties(ModifyPoint PROPERTIES PASS_REGULAR_EXPRESSION "Arg 0 .* struct. \{ int 6, double 2.3.* \}")

add_test(NAME ComplexStruct #
            COMMAND cliffi ${TESTLIB} v test_complex_struct -S: -c a 4 1.1 0x33 -S: 8 2.2 :S -i 54 5.4 -pS: 7 9.9 :S :S )
set_tests_properties(ComplexStruct PROPERTIES PASS_REGULAR_EXPRESSION "s.c: a")
set_tests_properties(ComplexStruct PROPERTIES PASS_REGULAR_EXPRESSION "s.x: 4")
set_tests_properties(ComplexStruct PROPERTIES PASS_REGULAR_EXPRESSION "s.y: 1.10*")
set_tests_properties(ComplexStruct PROPERTIES PASS_REGULAR_EXPRESSION "s.c2: 3")
set_tests_properties(ComplexStruct PROPERTIES PASS_REGULAR_EXPRESSION "s.p->x: 8")
set_tests_properties(ComplexStruct PROPERTIES PASS_REGULAR_EXPRESSION "s.p->y: 2.20*")
set_tests_properties(ComplexStruct PROPERTIES PASS_REGULAR_EXPRESSION "s.x2: 54")
set_tests_properties(ComplexStruct PROPERTIES PASS_REGULAR_EXPRESSION "s.y2: 5.40*")
set_tests_properties(ComplexStruct PROPERTIES PASS_REGULAR_EXPRESSION "s.p2.x: 7")
set_tests_properties(ComplexStruct PROPERTIES PASS_REGULAR_EXPRESSION "s.p2.y: 9.90*")

add_test(NAME Complex_P_Struct #
            COMMAND cliffi ${TESTLIB} v test_p_complex_struct -pS: -c a 4 1.1 0x33 -S: 8 2.2 :S -i 54 5.4 -pS: 7 9.9 :S :S )
set_tests_properties(Complex_P_Struct PROPERTIES PASS_REGULAR_EXPRESSION "s->c: a")
set_tests_properties(Complex_P_Struct PROPERTIES PASS_REGULAR_EXPRESSION "s->x: 4")
set_tests_properties(Complex_P_Struct PROPERTIES PASS_REGULAR_EXPRESSION "s->y: 1.10*")
set_tests_properties(Complex_P_Struct PROPERTIES PASS_REGULAR_EXPRESSION "s->c2: 3")
set_tests_properties(Complex_P_Struct PROPERTIES PASS_REGULAR_EXPRESSION "s->p->x: 8")
set_tests_properties(Complex_P_Struct PROPERTIES PASS_REGULAR_EXPRESSION "s->p->y: 2.20*")
set_tests_properties(Complex_P_Struct PROPERTIES PASS_REGULAR_EXPRESSION "s->x2: 54")
set_tests_properties(Complex_P_Struct PROPERTIES PASS_REGULAR_EXPRESSION "s->y2: 5.40*")
set_tests_properties(Complex_P_Struct PROPERTIES PASS_REGULAR_EXPRESSION "s->p2.x: 7")
set_tests_properties(Complex_P_Struct PROPERTIES PASS_REGULAR_EXPRESSION "s->p2.y: 9.90*")

add_test(NAME NestedLargerStruct #
COMMAND cliffi ${TESTLIB} v test_nested_large_struct -S: -c a -S: 5 3.3 -c b thisisastring :S 77 :S )
set_tests_properties(NestedLargerStruct PROPERTIES PASS_REGULAR_EXPRESSION "s.c: a")
set_tests_properties(NestedLargerStruct PROPERTIES PASS_REGULAR_EXPRESSION "s.s.x: 5")
set_tests_properties(NestedLargerStruct PROPERTIES PASS_REGULAR_EXPRESSION "s.s.y: 3.30*")
set_tests_properties(NestedLargerStruct PROPERTIES PASS_REGULAR_EXPRESSION "s.s.c: b")
set_tests_properties(NestedLargerStruct PROPERTIES PASS_REGULAR_EXPRESSION "s.s.s: thisisastring")
set_tests_properties(NestedLargerStruct PROPERTIES PASS_REGULAR_EXPRESSION "s.x: 77")

add_test(NAME return_struct #
COMMAND cliffi ${TESTLIB} S: i d :S get_point 4 2.2 )
set_tests_properties(return_struct PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: .*\{ int 4, double 2.2.*")

add_test(NAME return_struct_pointer #
COMMAND cliffi ${TESTLIB} pS: i d :S get_p_point 4 2.2 )
set_tests_properties(return_struct_pointer PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: .*\{ int 4, double 2.2.*")

add_test(NAME return_larger_struct #
COMMAND cliffi ${TESTLIB} S: i d c s :S get_larger_struct 4 2.2 -c q thisisastring )
set_tests_properties(return_larger_struct PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: .*\{ int 4, double 2.20*, char \'?q\'?, cstring \"thisisastring\"")

add_test(NAME return_larger_struct_pointer #
COMMAND cliffi ${TESTLIB} pS: i d c s :S get_p_larger_struct 4 2.2 -c q thisisastring )
set_tests_properties(return_larger_struct_pointer PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: .*\{ int 4, double 2.20*, char \'?q\'?, cstring \"thisisastring\"")

add_test(NAME return_nested_larger_struct #
COMMAND cliffi ${TESTLIB} S: c S: i d c s :S i :S get_nested_larger_struct -c a 5 3.3 -c b thisisastring 77 )
set_tests_properties(return_nested_larger_struct PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: .*\{ char \'?a\'?, .* \{ int 5, double 3.3.*, char \'?b\'?, cstring \"thisisastring\" \}, int 77.*")

add_test(NAME return_p_nested_larger_struct #
COMMAND cliffi ${TESTLIB} pS: c S: i d c s :S i :S get_p_nested_larger_struct -c a 5 3.3 -c b thisisastring 77 )
set_tests_properties(return_p_nested_larger_struct PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: .*\{ char \'?a\'?, .* \{ int 5, double 3.3.*, char \'?b\'?, cstring \"thisisastring\" \}, int 77.*")

add_test(NAME return_complex_struct #
# ComplexStruct get_complex_struct(unsigned char c, int x, double y, unsigned char c2, int x2, double y2, int x3, double y3){
COMMAND cliffi ${TESTLIB} S: c i d C S: i d :S i d pS: i d :S :S get_complex_struct -c 1 1 1.1 -C 0x32 2 2.2 3 3.3 4 4.4 )
set_tests_properties(return_complex_struct PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \{ char \'?1\'?, int 1, double 1.10*, uchar [^,]+, .*\{ int 2, double 2.20* \}, int 3, double 3.30*, .*\{ int 4, double 4.40* \}")

add_test(NAME return_p_complex_struct #
# ComplexStruct get_complex_struct(unsigned char c, int x, double y, unsigned char c2, int x2, double y2, int x3, double y3){
    COMMAND cliffi ${TESTLIB} pS: c i d C S: i d :S i d pS: i d :S :S get_p_complex_struct -c 1 1 1.1 -C 0x32 2 2.2 3 3.3 4 4.4 )
set_tests_properties(return_p_complex_struct PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \{ char \'?1\'?, int 1, double 1.10*, uchar [^,]+, .*\{ int 2, double 2.20* \}, int 3, double 3.30*, .*\{ int 4, double 4.40* \}")

add_test(NAME test_struct_with_embedded_array #
    # COMMAND cliffi ${TESTLIB} v test_struct_with_embedded_array -S: 1 -ac10 h,e,l,l,o 1,2,3 4 )
COMMAND cliffi ${TESTLIB} v test_struct_with_embedded_array -S: 1 -ac10 h,e,l,l,o 1,2,3 4 :S )
set_tests_properties(test_struct_with_embedded_array PROPERTIES PASS_REGULAR_EXPRESSION "s.x: 1")
set_tests_properties(test_struct_with_embedded_array PROPERTIES PASS_REGULAR_EXPRESSION "s.a[0]: 1")
set_tests_properties(test_struct_with_embedded_array PROPERTIES PASS_REGULAR_EXPRESSION "s.a[1]: 2")
set_tests_properties(test_struct_with_embedded_array PROPERTIES PASS_REGULAR_EXPRESSION "s.a[2]: 3")
set_tests_properties(test_struct_with_embedded_array PROPERTIES PASS_REGULAR_EXPRESSION "s.y: 4")
set_tests_properties(test_struct_with_embedded_array PROPERTIES PASS_REGULAR_EXPRESSION "s.s: hello")

add_test(NAME test_p_struct_with_embedded_array #
    COMMAND cliffi ${TESTLIB} v test_p_struct_with_embedded_array -pS: 1 -ac10 h,e,l,l,o 1,2,3 4 :S )
set_tests_properties(test_p_struct_with_embedded_array PROPERTIES PASS_REGULAR_EXPRESSION "s->x: 1")
set_tests_properties(test_p_struct_with_embedded_array PROPERTIES PASS_REGULAR_EXPRESSION "s->s: hello")
set_tests_properties(test_p_struct_with_embedded_array PROPERTIES PASS_REGULAR_EXPRESSION "s->a[0]: 1")
set_tests_properties(test_p_struct_with_embedded_array PROPERTIES PASS_REGULAR_EXPRESSION "s->a[1]: 2")
set_tests_properties(test_p_struct_with_embedded_array PROPERTIES PASS_REGULAR_EXPRESSION "s->a[2]: 3")
set_tests_properties(test_p_struct_with_embedded_array PROPERTIES PASS_REGULAR_EXPRESSION "s->y: 4")

add_test(NAME test_p_struct_with_embedded_array_intonly #
    COMMAND cliffi ${TESTLIB} v test_p_struct_with_embedded_array_intonly -pS: 1 1,2,3 4 :S )
set_tests_properties(test_p_struct_with_embedded_array_intonly PROPERTIES PASS_REGULAR_EXPRESSION "s->x: 1")
set_tests_properties(test_p_struct_with_embedded_array_intonly PROPERTIES PASS_REGULAR_EXPRESSION "s->s: hello")
set_tests_properties(test_p_struct_with_embedded_array_intonly PROPERTIES PASS_REGULAR_EXPRESSION "s->a[0]: 1")
set_tests_properties(test_p_struct_with_embedded_array_intonly PROPERTIES PASS_REGULAR_EXPRESSION "s->a[1]: 2")
set_tests_properties(test_p_struct_with_embedded_array_intonly PROPERTIES PASS_REGULAR_EXPRESSION "s->a[2]: 3")
set_tests_properties(test_p_struct_with_embedded_array_intonly PROPERTIES PASS_REGULAR_EXPRESSION "s->y: 4")
set_tests_properties(test_p_struct_with_embedded_array_intonly PROPERTIES PASS_REGULAR_EXPRESSION "Arg 0 after function .*1, 2, 3")

add_test(NAME test_p_struct_with_embedded_array_intonly_return_struct #
    COMMAND cliffi ${TESTLIB} S: i ai3 i :S test_p_struct_with_embedded_array_intonly_return_struct -pS: 1 1,2,3 4 :S )
set_tests_properties(test_p_struct_with_embedded_array_intonly_return_struct PROPERTIES PASS_REGULAR_EXPRESSION "s->x: 1")
set_tests_properties(test_p_struct_with_embedded_array_intonly_return_struct PROPERTIES PASS_REGULAR_EXPRESSION "s->a[0]: 1")
set_tests_properties(test_p_struct_with_embedded_array_intonly_return_struct PROPERTIES PASS_REGULAR_EXPRESSION "s->a[1]: 2")
set_tests_properties(test_p_struct_with_embedded_array_intonly_return_struct PROPERTIES PASS_REGULAR_EXPRESSION "s->a[2]: 3")
set_tests_properties(test_p_struct_with_embedded_array_intonly_return_struct PROPERTIES PASS_REGULAR_EXPRESSION "s->y: 4")

add_test(NAME return_struct_with_embedded_array_intonly #
    COMMAND cliffi ${TESTLIB} S: i ai3 i :S return_struct_with_embedded_array_intonly )
set_tests_properties(return_struct_with_embedded_array_intonly PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \{ int 1, .*2, 3, 4.* int 5")


add_test(NAME test_p_struct_with_embedded_array_charonly #
    COMMAND cliffi ${TESTLIB} v test_p_struct_with_embedded_array_charonly -pS: 1 -ac10 h,e,l,l,o 4 :S )
set_tests_properties(test_p_struct_with_embedded_array_charonly PROPERTIES PASS_REGULAR_EXPRESSION "s->x: 1")
set_tests_properties(test_p_struct_with_embedded_array_charonly PROPERTIES PASS_REGULAR_EXPRESSION "s->s: hello")
set_tests_properties(test_p_struct_with_embedded_array_charonly PROPERTIES PASS_REGULAR_EXPRESSION "s->y: 4")

add_test(NAME return_simple_struct_embedded_array #
    COMMAND cliffi ${TESTLIB} S: ai3 :S return_simple_struct_embedded_array )
set_tests_properties(return_simple_struct_embedded_array PROPERTIES PASS_REGULAR_EXPRESSION "Function returned:.*1, 2, 3.*")


add_test(NAME test_p_struct_with_embedded_array_changed #
    COMMAND cliffi ${TESTLIB} v test_p_struct_with_embedded_array_changed -pS: 1 -ac10 h,e,l,l,o 1,2,3 4 :S )
set_tests_properties(test_p_struct_with_embedded_array_changed PROPERTIES PASS_REGULAR_EXPRESSION "5.*changed.*10, 11, 12.*15")



add_test(NAME test_fixed_char_array_10_with_incomplete_values #
    COMMAND cliffi ${TESTLIB} v test_fixed_buffer_10_chars_arg_then_double_arg -ac10 h,e,l,l,o 4.4 )
set_tests_properties(test_fixed_char_array_10_with_incomplete_values PROPERTIES PASS_REGULAR_EXPRESSION "s: hello")
set_tests_properties(test_fixed_char_array_10_with_incomplete_values PROPERTIES PASS_REGULAR_EXPRESSION "d: 4.40*")

add_test(NAME test_fixed_char_array_10_with_full_values_then_double #
    COMMAND cliffi ${TESTLIB} v test_fixed_buffer_10_chars_arg_then_double_arg -ac10 h,e,l,l,o,a,b,c,d,0x00 4.4 )
set_tests_properties(test_fixed_char_array_10_with_full_values_then_double PROPERTIES PASS_REGULAR_EXPRESSION "s: helloabcd")
set_tests_properties(test_fixed_char_array_10_with_full_values_then_double PROPERTIES PASS_REGULAR_EXPRESSION "d: 4.40*")

add_test(NAME test_simple_struct_embedded_array_10_chars_filled_then_double #
    COMMAND cliffi ${TESTLIB} v test_simple_struct_embedded_array_10_chars_then_double -S: -ac10 h,e,l,l,o,a,b,c,d,0x00 4.4 :S )
set_tests_properties(test_simple_struct_embedded_array_10_chars_filled_then_double PROPERTIES PASS_REGULAR_EXPRESSION "s: helloabcd")
set_tests_properties(test_simple_struct_embedded_array_10_chars_filled_then_double PROPERTIES PASS_REGULAR_EXPRESSION "d: 4.40*")

add_test(NAME test_simple_struct_embedded_array_10_chars_then_double #
    COMMAND cliffi ${TESTLIB} v test_simple_struct_embedded_array_10_chars_then_double -S: -ac10 h,e,l,l,o,0x00,q 4.4 :S )
    set_tests_properties(test_simple_struct_embedded_array_10_chars_then_double PROPERTIES PASS_REGULAR_EXPRESSION "s: hello")
    set_tests_properties(test_simple_struct_embedded_array_10_chars_then_double PROPERTIES PASS_REGULAR_EXPRESSION "d: 4.40*")

add_test(NAME test_even_simpler_struct_embedded_array_10_chars_unfilled
    COMMAND cliffi ${TESTLIB} v test_even_simpler_struct_embedded_array_10_chars -S: -ac10 h,e,l,l,o,0x00,q :S )
set_tests_properties(test_even_simpler_struct_embedded_array_10_chars_unfilled PROPERTIES PASS_REGULAR_EXPRESSION "s: hello")

add_test(NAME test_even_simpler_struct_embedded_array_10_chars_filled
    COMMAND cliffi ${TESTLIB} v test_even_simpler_struct_embedded_array_10_chars -S: -ac10 h,e,l,l,o,a,b,c,d,0x00 :S )
set_tests_properties(test_even_simpler_struct_embedded_array_10_chars_filled PROPERTIES PASS_REGULAR_EXPRESSION "s: helloabcd")


add_test(NAME test_struct_with_embedded_array_pointer #
    COMMAND cliffi ${TESTLIB} v test_struct_with_char_pointer -S: -pac10 h,e,l,l,o,0x00,q :S )
set_tests_properties(test_struct_with_embedded_array_pointer PROPERTIES PASS_REGULAR_EXPRESSION "s: hello")

add_test(NAME test_p_struct_with_embedded_array_pointer #
    COMMAND cliffi ${TESTLIB} v test_p_struct_with_char_pointer -pS: -pac10 h,e,l,l,o,0x00,q :S )
set_tests_properties(test_p_struct_with_embedded_array_pointer PROPERTIES PASS_REGULAR_EXPRESSION "s: hello")
set_tests_properties(test_p_struct_with_embedded_array_pointer PROPERTIES PASS_REGULAR_EXPRESSION "after function return:.*hello")




add_test(NAME func_with_int_varargs #
    COMMAND cliffi ${TESTLIB} i sum_func_with_int_varargs 10 ... 1 2 3 4 5 6 7 8 9 10)
set_tests_properties(func_with_int_varargs PROPERTIES PASS_REGULAR_EXPRESSION "got vararg 0: 1")
set_tests_properties(func_with_int_varargs PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 55")

add_test(NAME varargs_structs #
    COMMAND cliffi ${TESTLIB} v varargs_structs 3 ... -S: 1 2.2 :S -S: 3 4.4 :S -S: 5 6.6 :S )
set_tests_properties(varargs_structs PROPERTIES PASS_REGULAR_EXPRESSION "got vararg 0: 1, 2.2.*")
set_tests_properties(varargs_structs PROPERTIES PASS_REGULAR_EXPRESSION "got vararg 1: 3, 4.4.*")
set_tests_properties(varargs_structs PROPERTIES PASS_REGULAR_EXPRESSION "got vararg 2: 5, 6.6.*")

add_test(NAME varargs_p_structs #
    COMMAND cliffi ${TESTLIB} v varargs_p_structs 3 ... -pS: 1 2.2 :S -pS: 3 4.4 :S -pS: 5 6.6 :S )
set_tests_properties(varargs_p_structs PROPERTIES PASS_REGULAR_EXPRESSION "got vararg 0: 1, 2.2.*")
set_tests_properties(varargs_p_structs PROPERTIES PASS_REGULAR_EXPRESSION "got vararg 1: 3, 4.4.*")
set_tests_properties(varargs_p_structs PROPERTIES PASS_REGULAR_EXPRESSION "got vararg 2: 5, 6.6.*")

add_test(NAME my_printf_novarargs #
    COMMAND cliffi ${TESTLIB} v my_printf -s "Hello" ...)
set_tests_properties(my_printf_novarargs PROPERTIES PASS_REGULAR_EXPRESSION "Hello")

add_test(NAME my_printf_string #
    COMMAND cliffi ${TESTLIB} v my_printf -s "Hello, %s!" ... "cliffi")
set_tests_properties(my_printf_string PROPERTIES PASS_REGULAR_EXPRESSION "Hello, cliffi!")

add_test(NAME my_printf_with_intonly #
    COMMAND cliffi ${TESTLIB} v my_printf -s "Hello, %d" ... 7)
set_tests_properties(my_printf_with_intonly PROPERTIES PASS_REGULAR_EXPRESSION "Hello, 7")

add_test(NAME my_printf_with_int #
    COMMAND cliffi ${TESTLIB} v my_printf -s "Hello, %s! %d" ... "cliffi" 5)
set_tests_properties(my_printf_with_int PROPERTIES PASS_REGULAR_EXPRESSION "Hello, cliffi! 5")

add_test(NAME my_printf_with_double #
    COMMAND cliffi ${TESTLIB} v my_printf -s "Hello, %s! %f" ... "cliffi" 5.5)
set_tests_properties(my_printf_with_double PROPERTIES PASS_REGULAR_EXPRESSION "Hello, cliffi! 5.50*")

add_test(NAME my_printf_with_double_and_int #
    COMMAND cliffi ${TESTLIB} v my_printf -s "Hello, %s! %f %d" ... "cliffi" 5.5 5)
set_tests_properties(my_printf_with_double_and_int PROPERTIES PASS_REGULAR_EXPRESSION "Hello, cliffi! 5.50* 5")

add_test(NAME my_printf_with_double_and_int_and_string #
    COMMAND cliffi ${TESTLIB} v my_printf -s "Hello, %s! %f %d %s" ... "cliffi" 5.5 5 "cliffi")
set_tests_properties(my_printf_with_double_and_int_and_string PROPERTIES PASS_REGULAR_EXPRESSION "Hello, cliffi! 5.50* 5 cliffi")

add_test(NAME my_printf_with_double_and_int_and_string_and_int #
    COMMAND cliffi ${TESTLIB} v my_printf -s "Hello, %s! %f %d %s %d" ... "cliffi" 5.5 5 "cliffi" 3)
set_tests_properties(my_printf_with_double_and_int_and_string_and_int PROPERTIES PASS_REGULAR_EXPRESSION "Hello, cliffi! 5.50* 5 cliffi 3")

add_test(NAME my_printf_with_char #
    COMMAND cliffi ${TESTLIB} v my_printf -s "Hello, %c" ... -c q)
set_tests_properties(my_printf_with_char PROPERTIES PASS_REGULAR_EXPRESSION "Hello, q")

add_test(NAME my_printf_with_double_and_int_and_string_and_char #
    COMMAND cliffi ${TESTLIB} v my_printf -s "Hello, %s! %f %d %s %c" ... "cliffi" 5.5 5 "cliffi" -c z)
set_tests_properties(my_printf_with_double_and_int_and_string_and_char PROPERTIES PASS_REGULAR_EXPRESSION "Hello, cliffi! 5.50* 5 cliffi z")

add_test(NAME my_printf_with_double_and_int_and_string_and_char_and_string #
    COMMAND cliffi ${TESTLIB} v my_printf -s "Hello, %s! %f %d %s %c %s" ... "cliffi" 5.5 5 "cliffi" -c z "cliffi")
set_tests_properties(my_printf_with_double_and_int_and_string_and_char_and_string PROPERTIES PASS_REGULAR_EXPRESSION "Hello, cliffi! 5.50* 5 cliffi z cliffi")

add_test(NAME my_printf_with_char_and_uchar_and_short_and_ushort_and_float # test all the types that need upgrading
    COMMAND cliffi ${TESTLIB} v my_printf -s "Hello, %c %c %hd %hu %f" ... -c q -C 0x32 -h 3 -H 4 -f 5.5)
set_tests_properties(my_printf_with_char_and_uchar_and_short_and_ushort_and_float PROPERTIES PASS_REGULAR_EXPRESSION "Hello, q 2 3 4 5.50*")

add_test(NAME my_printf_with_p_char_and_p_uchar_and_p_short_and_p_ushort_and_p_float_and_p_int_and_p_double_and_p_struct # test with a bunch of pointers
    COMMAND cliffi ${TESTLIB} v my_printf -s "Hello, %p %p %p %p %p %p %p %p" ... -pc q -pC 0x32 -ph 3 -pH 4 -pf 5.5 -pi 6 -pd 7.7 -pS: 8 9.9 :S)
# can't really test the pointers, but we can test that they are printed. we're using very liberal pointer formatting regex here to allow for different pointer formats
set_tests_properties(my_printf_with_p_char_and_p_uchar_and_p_short_and_p_ushort_and_p_float_and_p_int_and_p_double_and_p_struct PROPERTIES PASS_REGULAR_EXPRESSION "Hello, 0.* 0.* 0.* 0.* 0.* 0.* 0.* 0.*")


#test that short types are okay as returns
add_test(NAME get_char #
    COMMAND cliffi ${TESTLIB} c get_char)
set_tests_properties(get_char PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \'?a\'?")

add_test(NAME get_short #
    COMMAND cliffi ${TESTLIB} h get_short)
set_tests_properties(get_short PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 1")

add_test(NAME get_float #
    COMMAND cliffi ${TESTLIB} f get_float)
set_tests_properties(get_float PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 1.50*")

add_test(NAME get_short_negative5 #
    COMMAND cliffi ${TESTLIB} h get_short_negative5)
set_tests_properties(get_short_negative5 PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: -5")

add_test(NAME get_uchar_255 #
    COMMAND cliffi ${TESTLIB} C get_uchar_255)
set_tests_properties(get_uchar_255 PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 255")

add_test(NAME get_ushort_65535 #
    COMMAND cliffi ${TESTLIB} H get_ushort_65535)
set_tests_properties(get_ushort_65535 PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 65535")


add_test(NAME return_some_numbers_via_sizet_outparam #
    COMMAND cliffi ${TESTLIB} v return_some_numbers -pait2 null -pi null)
set_tests_properties(return_some_numbers_via_sizet_outparam PROPERTIES PASS_REGULAR_EXPRESSION "\{ 1, 2, 3 \}")
add_test(NAME TestNegativeInts
COMMAND cliffi ${TESTLIB} i add -99 -1)
set_tests_properties(TestNegativeInts PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: -100")

add_test(NAME TestLongInts
         COMMAND cliffi ${TESTLIB} l add_long -l -5000 -l -5000)
set_tests_properties(TestLongInts PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: -10000")

add_test(NAME TestUnsignedLongs
         COMMAND cliffi ${TESTLIB} L add_ulong -L 2147483647 -L 2147483647)
set_tests_properties(TestUnsignedLongs PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 4294967294")

add_test(NAME TestDoublePointers
COMMAND cliffi ${TESTLIB} i increment_at_pointer_pointer -ppi 42)

add_test(NAME TestStructWithEmbeddedArray
COMMAND cliffi ${TESTLIB} v test_struct_with_embedded_array -S: 42 -ac10 h,e,l,l,o,0x00 1,2,3 99 :S)
set_tests_properties(TestStructWithEmbeddedArray PROPERTIES PASS_REGULAR_EXPRESSION "s\\.x: 42.*s\\.s:.*hello.*s\\.a\\[0\\]: 1.*s\\.a\\[1\\]: 2.*s\\.a\\[2\\]: 3.*s\\.y: 99")


add_test(NAME TestComplexVarargs
COMMAND cliffi ${TESTLIB} v my_printf -s "integers: %d %d %d, strings: %s %s, floats: %f %f, char: %c" ... -i 1 -i 2 -i 3 -s foo -s bar -f 1.23 -d 4.56 -c z)
set_tests_properties(TestComplexVarargs PROPERTIES PASS_REGULAR_EXPRESSION "integers: 1 2 3, strings: foo bar, floats: 1.23.* 4.56.*, char: z")

add_test(NAME TestUnsignedTypes
COMMAND cliffi ${TESTLIB} v my_printf -s "unsigned: %hhu %hu %u %lu" ... -C 255 -H 65535 -I 4294967295 -L 4294967295)
set_tests_properties(TestUnsignedTypes PROPERTIES PASS_REGULAR_EXPRESSION "unsigned: 255 65535 4294967295 4294967295")

add_test(NAME return_struct_with_embedded_array_complex #
    COMMAND cliffi ${TESTLIB} S: i ac10 ai3 i :S return_struct_with_embedded_array )
set_tests_properties(return_struct_with_embedded_array_complex PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \{ int 1,.*h.?e.?l.?l.?o.?.*2, 3, 4.*int 5")

add_test(NAME return_struct_with_embedded_array_pointer
    COMMAND cliffi ${TESTLIB} S: pac10 :S return_struct_with_embedded_array_pointer)
set_tests_properties(return_struct_with_embedded_array_pointer PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: .*h.?e.?l.?l.?o.?")

add_test(NAME return_p_struct_with_embedded_array_pointer
    COMMAND cliffi ${TESTLIB} pS: pac10 :S return_p_struct_with_embedded_array_pointer)
set_tests_properties(return_p_struct_with_embedded_array_pointer PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: .*h.?e.?l.?l.?o.?")

add_test(NAME grand_test_of_arrays_of_int_pointers
    COMMAND cliffi ${TESTLIB} api12 grand_test_of_arrays_of_int_pointers -S: -api3 1,2,3 :S -pS: -api3 4,5,6 :S -api3 7,8,9 -papi3 10,11,12)
    set_tests_properties(grand_test_of_arrays_of_int_pointers PROPERTIES PASS_REGULAR_EXPRESSION "s.arr[0]: 1")
    set_tests_properties(grand_test_of_arrays_of_int_pointers PROPERTIES PASS_REGULAR_EXPRESSION "s.arr[1]: 2")
    set_tests_properties(grand_test_of_arrays_of_int_pointers PROPERTIES PASS_REGULAR_EXPRESSION "s.arr[2]: 3")
    set_tests_properties(grand_test_of_arrays_of_int_pointers PROPERTIES PASS_REGULAR_EXPRESSION "s2->arr[0]: 4")
    set_tests_properties(grand_test_of_arrays_of_int_pointers PROPERTIES PASS_REGULAR_EXPRESSION "s2->arr[1]: 5")
    set_tests_properties(grand_test_of_arrays_of_int_pointers PROPERTIES PASS_REGULAR_EXPRESSION "s2->arr[2]: 6")
    set_tests_properties(grand_test_of_arrays_of_int_pointers PROPERTIES PASS_REGULAR_EXPRESSION "arr[0]: 7")
    set_tests_properties(grand_test_of_arrays_of_int_pointers PROPERTIES PASS_REGULAR_EXPRESSION "arr[1]: 8")
    set_tests_properties(grand_test_of_arrays_of_int_pointers PROPERTIES PASS_REGULAR_EXPRESSION "arr[2]: 9")
    set_tests_properties(grand_test_of_arrays_of_int_pointers PROPERTIES PASS_REGULAR_EXPRESSION "arr2[0]: 10")
    set_tests_properties(grand_test_of_arrays_of_int_pointers PROPERTIES PASS_REGULAR_EXPRESSION "arr2[1]: 11")
    set_tests_properties(grand_test_of_arrays_of_int_pointers PROPERTIES PASS_REGULAR_EXPRESSION "arr2[2]: 12")
    set_tests_properties(grand_test_of_arrays_of_int_pointers PROPERTIES PASS_REGULAR_EXPRESSION "Function returned:.*1, 2, 3.*4, 5, 6.*7, 8, 9.*10, 11, 12")

add_test(NAME test_array_of_int_pointers_simple
    COMMAND cliffi ${TESTLIB} i test_array_of_int_pointers_simple -api3 1,2,3)
    set_tests_properties(test_array_of_int_pointers_simple PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 6")

add_test(NAME test_array_of_pointers_in_struct
    COMMAND cliffi ${TESTLIB} i test_array_of_pointers_in_struct -S: -api3 1,2,3 4,5,6 :S)
    set_tests_properties(test_array_of_pointers_in_struct PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 6")



add_test(NAME get_packed_struct #
    COMMAND cliffi ${TESTLIB} SK: c i ac2 d ai3 c :S get_packed_struct )

add_test(NAME get_p_packed_struct #
    COMMAND cliffi ${TESTLIB} pSK: c i ac2 d ai3 c :S get_p_packed_struct )

add_test(NAME test_packed_struct #
    COMMAND cliffi ${TESTLIB} v test_packed_struct -SK: -c a 0 a,b 4.5 1,2,3 -c q :S)
    set_tests_properties(test_packed_struct PROPERTIES PASS_REGULAR_EXPRESSION "s.c: a")
    set_tests_properties(test_packed_struct PROPERTIES PASS_REGULAR_EXPRESSION "s.i: 0")
    set_tests_properties(test_packed_struct PROPERTIES PASS_REGULAR_EXPRESSION "s.s[0]: a")
    set_tests_properties(test_packed_struct PROPERTIES PASS_REGULAR_EXPRESSION "s.s[1]: b")
    set_tests_properties(test_packed_struct PROPERTIES PASS_REGULAR_EXPRESSION "s.d: 4.50*")



add_test(NAME test_p_packed_struct #
    COMMAND cliffi ${TESTLIB} v test_p_packed_struct -pSK: -c a 0 a,b 4.5 1,2,3 -c q :S)
    set_tests_properties(test_p_packed_struct PROPERTIES PASS_REGULAR_EXPRESSION "s->c: a")
    set_tests_properties(test_p_packed_struct PROPERTIES PASS_REGULAR_EXPRESSION "s->i: 0")
    set_tests_properties(test_p_packed_struct PROPERTIES PASS_REGULAR_EXPRESSION "s->s[0]: a")
    set_tests_properties(test_p_packed_struct PROPERTIES PASS_REGULAR_EXPRESSION "s->s[1]: b")
    set_tests_properties(test_p_packed_struct PROPERTIES PASS_REGULAR_EXPRESSION "s->d: 4.50*")

add_test(NAME test_setting_array_with_long_hex
COMMAND cliffi ${TESTLIB} s concat -s "test" -ac 0x3031323334 )
set_tests_properties(test_setting_array_with_long_hex PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"?test01234\"?")


add_test(NAME test_null_pointer_syntax
COMMAND cliffi ${TESTLIB} i is_null Npi)
set_tests_properties(test_null_pointer_syntax PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 1")

if(USE_REPL_MODE)

add_test(NAME repl_smoketest #
COMMAND cliffi --repltest
${TESTLIB} i add 1 2 \n
${TESTLIB} i add 3 4 \n
)
set_tests_properties(repl_smoketest PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 3")
set_tests_properties(repl_smoketest PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 7")

add_test(NAME repl_metatest_tests_fail_on_bad_input
COMMAND cliffi --repltest
thisisnotagoodcommand \n
)
set_tests_properties(repl_metatest_tests_fail_on_bad_input PROPERTIES WILL_FAIL TRUE) # this test is expected to fail

add_test(NAME repl_metatest_tests_fail_on_segfault
COMMAND cliffi --repltest
null_pointer = -P 0 \n
${TESTLIB} i increment_at_pointer null_pointer \n
)
set_tests_properties(repl_metatest_tests_fail_on_segfault PROPERTIES WILL_FAIL TRUE) # this test is expected to fail

add_test(NAME repl_metatest_tests_fail_on_segfault_in_function_created_thread
COMMAND cliffi --repltest
${TESTLIB} v do_segfault_in_another_thread \n
)
set_tests_properties(repl_metatest_tests_fail_on_segfault_in_function_created_thread PROPERTIES WILL_FAIL TRUE) # this test is expected to fail

add_test(NAME repl_metatest_tests_dont_fail_on_bad_input_with_nofail_option
COMMAND cliffi --repltest --noexitonfail
thisisnotagoodcommand \n
should_reach_here = 1 \n
)
set_tests_properties(repl_metatest_tests_dont_fail_on_bad_input_with_nofail_option PROPERTIES PASS_REGULAR_EXPRESSION "should_reach_here = 1")

add_test(NAME repl_metatest_tests_dont_fail_on_segfault_with_nofail_option
COMMAND cliffi --repltest --noexitonfail
null_pointer = -P 0 \n
${TESTLIB} i increment_at_pointer null_pointer \n
should_reach_here = 1 \n
)
set_tests_properties(repl_metatest_tests_dont_fail_on_segfault_with_nofail_option PROPERTIES PASS_REGULAR_EXPRESSION "should_reach_here = 1")

add_test(NAME repl_metatest_tests_dont_fail_on_segfault_in_function_created_thread_with_nofail_option
COMMAND cliffi --repltest --noexitonfail
${TESTLIB} v do_segfault_in_another_thread \n
should_reach_here = 1 \n
)
set_tests_properties(repl_metatest_tests_dont_fail_on_segfault_in_function_created_thread_with_nofail_option PROPERTIES PASS_REGULAR_EXPRESSION "should_reach_here = 1")


add_test(NAME repl_test_globals #
COMMAND cliffi --repltest
${TESTLIB} i increment_global \n
${TESTLIB} i increment_global \n
${TESTLIB} i increment_global \n
)
set_tests_properties(repl_test_globals PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 1")
set_tests_properties(repl_test_globals PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 2")
set_tests_properties(repl_test_globals PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 3")

add_test(NAME repl_test_globals_after_libclose #
COMMAND cliffi --repltest
${TESTLIB} i increment_global \n
${TESTLIB} i increment_global \n
close ${TESTLIB}
${TESTLIB} i increment_global \n
)
set_tests_properties(repl_test_globals_after_libclose PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 1")
set_tests_properties(repl_test_globals_after_libclose PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 2")
set_tests_properties(repl_test_globals_after_libclose PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 1")

add_test(NAME repl_test_var_ac_with_0x0
COMMAND cliffi --repltest
set charbuffer -ac a,b,0x0,c,d,0x0 \n
print charbuffer \n
)
set_tests_properties(repl_test_var_ac_with_0x0 PROPERTIES PASS_REGULAR_EXPRESSION "charbuffer = ab\\\\0cd\\\\0") # \\\\ escapes a single backslash

add_test(NAME repl_test_accidentally_including_equals_in_set
COMMAND cliffi --repltest
set var1 = 3 \n
)
set_tests_properties(repl_test_accidentally_including_equals_in_set PROPERTIES PASS_REGULAR_EXPRESSION "int var1 = 3")

add_test(NAME repl_test_char_array_with_space
COMMAND cliffi --repltest
set charbuffer -ac a,b,\" \",d,e \n
)
set_tests_properties(repl_test_char_array_with_space PROPERTIES PASS_REGULAR_EXPRESSION "charbuffer = ab de")

add_test(NAME repl_test_variable_simple #
COMMAND cliffi --repltest
set var1 3 \n
set var2 7 \n
${TESTLIB} i add var1 var2 \n
)
set_tests_properties(repl_test_variable_simple PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 10")

add_test(NAME repl_test_variable_complex #
COMMAND cliffi --repltest
set foo -S: 1 -ac10 h,e,l,l,o 1,2,3 4 :S \n
${TESTLIB} v test_struct_with_embedded_array foo \n
)
set_tests_properties(repl_test_variable_complex PROPERTIES PASS_REGULAR_EXPRESSION "s.x: 1")
set_tests_properties(repl_test_variable_complex PROPERTIES PASS_REGULAR_EXPRESSION "s.a[0]: 1")
set_tests_properties(repl_test_variable_complex PROPERTIES PASS_REGULAR_EXPRESSION "s.a[1]: 2")
set_tests_properties(repl_test_variable_complex PROPERTIES PASS_REGULAR_EXPRESSION "s.a[2]: 3")
set_tests_properties(repl_test_variable_complex PROPERTIES PASS_REGULAR_EXPRESSION "s.y: 4")
set_tests_properties(repl_test_variable_complex PROPERTIES PASS_REGULAR_EXPRESSION "s.s: hello")

add_test(NAME repl_test_var_pointer #
COMMAND cliffi --repltest
set num -pi 1 \n
${TESTLIB} i increment_at_pointer num \n
print num \n
)
set_tests_properties(repl_test_var_pointer PROPERTIES PASS_REGULAR_EXPRESSION "int. num = 1")  # using int. instead of int*
set_tests_properties(repl_test_var_pointer PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 2")
set_tests_properties(repl_test_var_pointer PROPERTIES PASS_REGULAR_EXPRESSION "Arg 0 after.*2")
set_tests_properties(repl_test_var_pointer PROPERTIES PASS_REGULAR_EXPRESSION "int. num = 2")

add_test(NAME repl_test_voidpointer #)
COMMAND cliffi --repltest
set strvoidpointer -P 0 \n
${TESTLIB} strvoidpointer get_message \n
print strvoidpointer \n
${TESTLIB} s concat strvoidpointer strvoidpointer \n
)
set_tests_properties(repl_test_voidpointer PROPERTIES PASS_REGULAR_EXPRESSION "strvoidpointer = 0?x?[0-9a-f]{4,}")
set_tests_properties(repl_test_voidpointer PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"?Hello, cliffi.Hello, cliffi.\"?")


add_test(NAME repl_test_variable_simple_syntactic_sugar #
COMMAND cliffi --repltest
var1 = 3 \n
var2  =  7 \n
${TESTLIB} var2 add var1 var2 \n
var2 \n
)
set_tests_properties(repl_test_variable_simple_syntactic_sugar PROPERTIES PASS_REGULAR_EXPRESSION "int var2 = 7")
set_tests_properties(repl_test_variable_simple_syntactic_sugar PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 10")
set_tests_properties(repl_test_variable_simple_syntactic_sugar PROPERTIES PASS_REGULAR_EXPRESSION "int var2 = 10")

add_test(NAME repl_test_variable_set_to_another #)
COMMAND cliffi --repltest
set var1 3 \n
set var2 var1 \n
set var1 4 \n
var2 \n
)
set_tests_properties(repl_test_variable_set_to_another PROPERTIES FAIL_REGULAR_EXPRESSION "int var2 = 4")

add_test(NAME repl_test_tokenizer #
COMMAND cliffi --repltest
set var1 \"test string foo\" \n
set var2 a\\ b\\ c \n
${TESTLIB} s concat var1 \" and bar and \\ slash\" \n
print var2 \n
)
set_tests_properties(repl_test_tokenizer PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"?test string foo and bar and . slash")
set_tests_properties(repl_test_tokenizer PROPERTIES PASS_REGULAR_EXPRESSION "var2 = \"?a b c")

add_test(NAME repl_test_store_embedded_struct
COMMAND cliffi --repltest
set foo -S: abc -S: qwerty :S :S \n
set globalbuffer -P 0 \n
${TESTLIB} globalbuffer get_address_of_global_buffer1 \n
store globalbuffer foo \n
)
set_tests_properties(repl_test_store_embedded_struct PROPERTIES PASS_REGULAR_EXPRESSION "globalbuffer.*qwerty")

add_test(NAME repl_test_store_embedded_struct_three_times
COMMAND cliffi --repltest
set foo -S: abc -S: qwerty :S :S \n
set globalbuffer -P 0 \n
${TESTLIB} globalbuffer get_address_of_global_buffer1 \n
store globalbuffer foo \n
store globalbuffer foo \n
store globalbuffer foo \n
bar = foo \n
)
set_tests_properties(repl_test_store_embedded_struct_three_times PROPERTIES PASS_REGULAR_EXPRESSION "globalbuffer.*qwerty")
set_tests_properties(repl_test_store_embedded_struct_three_times PROPERTIES PASS_REGULAR_EXPRESSION "bar.*qwerty")


add_test(NAME repl_test_load_store_dump_memory
COMMAND cliffi --repltest
set global_address -P 0 \n
${TESTLIB} i increment_global \n
${TESTLIB} global_address get_address_of_global \n
global_address \n
load global_int i global_address \n
dump i global_address \n
store global_address 42 \n
dump i global_address \n
${TESTLIB} i increment_at_pointer global_address \n
dump i global_address \n
print global_int \n
load global_int i global_address \n
store global_address 88 \n
print global_int \n
)
set_tests_properties(repl_test_load_store_dump_memory PROPERTIES PASS_REGULAR_EXPRESSION "global_address = 0?x?[0-9a-f]{4,}")
set_tests_properties(repl_test_load_store_dump_memory PROPERTIES PASS_REGULAR_EXPRESSION "int global_int = 1")
set_tests_properties(repl_test_load_store_dump_memory PROPERTIES PASS_REGULAR_EXPRESSION "global_address = (int )?42")
set_tests_properties(repl_test_load_store_dump_memory PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 43")
set_tests_properties(repl_test_load_store_dump_memory PROPERTIES PASS_REGULAR_EXPRESSION "int global_int = 43")
set_tests_properties(repl_test_load_store_dump_memory PROPERTIES FAIL_REGULAR_EXPRESSION "int global_int = 88")


add_test(NAME repl_test_struct_variables_nested_structs # larger = {int double char char*}  containing = {char larger int}
COMMAND cliffi --repltest
set foo -S: 1 2.2 a hello :S \n
set containing -S: q foo 5 :S \n
${TESTLIB} v test_nested_large_struct containing \n
)
set_tests_properties(repl_test_struct_variables_nested_structs PROPERTIES PASS_REGULAR_EXPRESSION "s.c: q")
set_tests_properties(repl_test_struct_variables_nested_structs PROPERTIES PASS_REGULAR_EXPRESSION "s.s.x: 1")
set_tests_properties(repl_test_struct_variables_nested_structs PROPERTIES PASS_REGULAR_EXPRESSION "s.s.y: 2.20*")
set_tests_properties(repl_test_struct_variables_nested_structs PROPERTIES PASS_REGULAR_EXPRESSION "s.s.c: a")
set_tests_properties(repl_test_struct_variables_nested_structs PROPERTIES PASS_REGULAR_EXPRESSION "s.s.s: hello")
set_tests_properties(repl_test_struct_variables_nested_structs PROPERTIES PASS_REGULAR_EXPRESSION "s.x: 5")

add_test(NAME repl_test_store_load_struct_variables_nested_structs # larger = {int double char char*}  containing = {char larger int}
COMMAND cliffi --repltest
set foo -S: 1 2.2 a hello :S \n
set containing -S: q foo 5 :S \n
set globalbuffer -P 0 \n
${TESTLIB} globalbuffer get_address_of_global_buffer1 \n
store globalbuffer containing \n
dump S: c S: i d c s :S i :S globalbuffer \n
load loadedstruct S: c S: i d c s :S i :S globalbuffer \n
${TESTLIB} v test_nested_large_struct loadedstruct \n
)
#TODO: add a test for the dump command
set_tests_properties(repl_test_store_load_struct_variables_nested_structs PROPERTIES PASS_REGULAR_EXPRESSION "s.c: q")
set_tests_properties(repl_test_store_load_struct_variables_nested_structs PROPERTIES PASS_REGULAR_EXPRESSION "s.s.x: 1")
set_tests_properties(repl_test_store_load_struct_variables_nested_structs PROPERTIES PASS_REGULAR_EXPRESSION "s.s.y: 2.20*")
set_tests_properties(repl_test_store_load_struct_variables_nested_structs PROPERTIES PASS_REGULAR_EXPRESSION "s.s.c: a")
set_tests_properties(repl_test_store_load_struct_variables_nested_structs PROPERTIES PASS_REGULAR_EXPRESSION "s.s.s: hello")
set_tests_properties(repl_test_store_load_struct_variables_nested_structs PROPERTIES PASS_REGULAR_EXPRESSION "s.x: 5")
set_tests_properties(repl_test_store_load_struct_variables_nested_structs PROPERTIES PASS_REGULAR_EXPRESSION ".*0x[0-9a-f]*. = struct { char \'?q\'?, struct { int 1, double 2.20*, char \'?a\'?, cstring \"?hello\"? }, int 5 }")
set_tests_properties(repl_test_store_load_struct_variables_nested_structs PROPERTIES PASS_REGULAR_EXPRESSION "loadedstruct.* { char \'?q\'?, struct { int 1, double 2.20*, char \'?a\'?, cstring \"?hello\"? }, int 5 }")


# struct embedded_with_pointer {
#     int* x;
#     char* s;
# };

# struct parent_with_embedded_pointer {
#     char x;
#     struct embedded_with_pointer p;
#     struct embedded_with_pointer* p2;
#     int y;
# };

# void test_parent_with_embedded_pointer(struct parent_with_embedded_pointer s) {
#     hexdump(&s, sizeof(struct parent_with_embedded_pointer));
#     s.p.x[0]++;
#     //capitalize the string
#     for (int i = 0; i < strlen(s.p.s); i++) {
#         s.p.s[i] = toupper(s.p.s[i]);
#     }
#     s.p2->x[0]++;
#     //capitalize the string
#     for (int i = 0; i < strlen(s.p2->s); i++) {
#         s.p2->s[i] = toupper(s.p2->s[i]);
#     }
# }

add_test(NAME repl_test_struct_variables_nested_structs_with_pointers # inner = {int* char*}  outer = {char inner inner* int}
COMMAND cliffi --repltest
set inner -S: -pi 1 -s hello :S \n
set pinner2 -pS: -pi 2 -s world :S \n
set outer -S: a inner pinner2 5 :S \n
${TESTLIB} v test_parent_with_embedded_pointer outer \n
outer
)
set_tests_properties(repl_test_struct_variables_nested_structs_with_pointers PROPERTIES PASS_REGULAR_EXPRESSION "{ int\\* 2, cstring \"?HELLO\"? }.*{ int\\* 3, cstring \"?WORLD\"? }")

# struct simpler_parent_with_embedded_pointer {
#     char x;
#     struct embedded_with_pointer p;
# };

add_test(NAME repl_simple_test_int_pointer
COMMAND cliffi --repltest
set var -pi 1 \n
${TESTLIB} v simple_test_int_pointer var \n
var \n
)
set_tests_properties(repl_simple_test_int_pointer PROPERTIES PASS_REGULAR_EXPRESSION "Function returned:")
set_tests_properties(repl_simple_test_int_pointer PROPERTIES PASS_REGULAR_EXPRESSION "int\\* var = 2")

# void test_struct_with_int_pointer(struct with_int_pointer s) {
#     hexdump(&s, sizeof(struct with_int_pointer));
#     printf("s.x: %d\n", *s.x);
#     (*s.x)++;
# }

add_test(NAME repl_test_struct_with_int_pointer
COMMAND cliffi --repltest
set var -S: -pi 1 :S \n
${TESTLIB} v test_struct_with_int_pointer var \n
var \n
)
set_tests_properties(repl_test_struct_with_int_pointer PROPERTIES PASS_REGULAR_EXPRESSION "Function returned:")
set_tests_properties(repl_test_struct_with_int_pointer PROPERTIES PASS_REGULAR_EXPRESSION "{ int\\* 2 }")

add_test(NAME nonrepl_test_struct_with_int_pointer
COMMAND cliffi ${TESTLIB} v test_struct_with_int_pointer -S: -pi 1 :S \n
)
set_tests_properties(nonrepl_test_struct_with_int_pointer PROPERTIES PASS_REGULAR_EXPRESSION "Function returned:")


add_test(NAME repl_test_struct_variables_nested_structs_with_pointers_simpler # inner = {int* char*}  outer = {char inner}
COMMAND cliffi --repltest
set inner -S: -pi 1 -s hello :S \n
set outer -S: a inner :S \n
${TESTLIB} v simple_test_parent_with_embedded_pointer outer \n
outer
)
set_tests_properties(repl_test_struct_variables_nested_structs_with_pointers_simpler PROPERTIES PASS_REGULAR_EXPRESSION "Function returned:")
set_tests_properties(repl_test_struct_variables_nested_structs_with_pointers_simpler PROPERTIES PASS_REGULAR_EXPRESSION "{ int\\* 2, cstring \"?HELLO\"? }")


add_test(NAME repl_test_struct_variables_nested_structs_with_pointers_simpler_pointer # inner = {int* char*}  outer = {char *inner})
COMMAND cliffi --repltest
set pinner -pS: -pi 1 -s hello :S \n
set outer -S: a pinner :S \n
${TESTLIB} v simple_test_parent_with_pointer_struct_embedded_pointer outer \n
outer
)
set_tests_properties(repl_test_struct_variables_nested_structs_with_pointers_simpler_pointer PROPERTIES PASS_REGULAR_EXPRESSION "Function returned:")
set_tests_properties(repl_test_struct_variables_nested_structs_with_pointers_simpler_pointer PROPERTIES PASS_REGULAR_EXPRESSION "{ int\\* 2, cstring \"?HELLO\"? }")

add_test(NAME repl_test_struct_variables_nested_structs_with_pointers_simpler_pointer_fewerlines # inner = {int* char*}  outer = {char *inner})
COMMAND cliffi --repltest
set outer -S: a -pS: -pi 1 -s hello :S :S \n
${TESTLIB} v simple_test_parent_with_pointer_struct_embedded_pointer outer \n
outer
)
set_tests_properties(repl_test_struct_variables_nested_structs_with_pointers_simpler_pointer_fewerlines PROPERTIES PASS_REGULAR_EXPRESSION "Function returned:")
set_tests_properties(repl_test_struct_variables_nested_structs_with_pointers_simpler_pointer_fewerlines PROPERTIES PASS_REGULAR_EXPRESSION "{ int\\* 2, cstring \"?HELLO\"? }")




add_test(NAME repl_test_address_addition # larger = {int double char char*}  containing = {char larger int}
COMMAND cliffi --repltest
set global_address -P 0 \n
${TESTLIB} global_address get_global_string \n
store global_address+1 a \n
offset8 = 8 \n
P3 = -P 3 \n
store global_address+offset8 y \n # 8 bytes
store global_address+2*P3 _ \n # 6 bytes
store global_address+2+5*2 -c : \n # 12 bytes
${TESTLIB} s get_global_string \n
)
set_tests_properties(repl_test_address_addition PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"?Hallo,_wyrld:")

add_test(NAME repl_test_getx_var_after_function
COMMAND cliffi --repltest
foo = -S: 5 2.2 :S \n
${TESTLIB} i get_x foo \n
foo
)
set_tests_properties(repl_test_getx_var_after_function PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 5")
set_tests_properties(repl_test_getx_var_after_function PROPERTIES FAIL_REGULAR_EXPRESSION "segv_handler")

add_test(NAME repl_test_getx_var_after_function_twice
COMMAND cliffi --repltest
foo = -S: 5 2.2 :S \n
${TESTLIB} i get_x foo \n
${TESTLIB} i get_x foo \n
${TESTLIB} i get_x foo \n
foo2 = foo
)
set_tests_properties(repl_test_getx_var_after_function_twice PROPERTIES FAIL_REGULAR_EXPRESSION "Function returned: [^5]|5.")
set_tests_properties(repl_test_getx_var_after_function_twice PROPERTIES FAIL_REGULAR_EXPRESSION "segv_handler")
set_tests_properties(repl_test_getx_var_after_function_twice PROPERTIES PASS_REGULAR_EXPRESSION "foo2.*{ int 5, double 2.2.* }")


add_test(NAME repl_test_modifypoint_var_after_function #
COMMAND cliffi --repltest
foo = -pS: 5 2.2 :S \n
${TESTLIB} v modify_point foo 1 .1 \n
foo
)
set_tests_properties(repl_test_modifypoint_var_after_function PROPERTIES PASS_REGULAR_EXPRESSION "Arg 0 .* struct. \{ int 6, double 2.3.* \}")
set_tests_properties(repl_test_modifypoint_var_after_function PROPERTIES PASS_REGULAR_EXPRESSION "Function returned:")
set_tests_properties(repl_test_modifypoint_var_after_function PROPERTIES PASS_REGULAR_EXPRESSION "foo.*\{ int 6, double 2.3.* \}")


add_test(NAME callFunctionByAddress
COMMAND cliffi --repltest
getMesgFuncAddr = -P 0 \n
${TESTLIB} getMesgFuncAddr getAddressOfGetMessage \n
${TESTLIB} s getMesgFuncAddr \n
)
set_tests_properties(callFunctionByAddress PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"?Hello, cliffi.\"?")


find_program(NM_COMMAND nm)
if(NM_COMMAND)

function(find_symbol_offset LIBRARY_PATH SYMBOL_NAME OFFSET_VARIABLE)
    # Command to find the symbol offset
    execute_process(
        COMMAND ${NM_COMMAND} -D ${LIBRARY_PATH}
        OUTPUT_VARIABLE NM_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # message(STATUS "nm output: ${NM_OUTPUT}")
    set(SYMBOL_OUTPUT "")
    string(REGEX MATCH "[0-9a-fA-F]+ [a-zA-Z] _?${SYMBOL_NAME}" SYMBOL_OUTPUT "${NM_OUTPUT}")

    # Debug output to verify the command worked
    message(STATUS "${SYMBOL_NAME} Symbol output: ${SYMBOL_OUTPUT}")

    # Parse the output to extract the offset
    string(REGEX MATCH "^[0-9a-fA-F]+" SYMBOL_OFFSET "${SYMBOL_OUTPUT}")

    # Verify that the offset was found
    if(NOT SYMBOL_OFFSET)
        message(FATAL_ERROR "Symbol ${SYMBOL_NAME} not found in ${LIBRARY_PATH}")
    endif()

    #Remove extra preceeding 0s
    string(REGEX REPLACE "^0+" "" SYMBOL_OFFSET "${SYMBOL_OFFSET}")

    # Ensure the offset is prefixed with "0x"
    if(NOT SYMBOL_OFFSET MATCHES "^0x")
        set(SYMBOL_OFFSET "0x${SYMBOL_OFFSET}")
    endif()


    # Debug output to verify the extracted offset
    message(STATUS "${SYMBOL_NAME} offset: ${SYMBOL_OFFSET}")
    # Set the output variable
    set(${OFFSET_VARIABLE} ${SYMBOL_OFFSET} PARENT_SCOPE)
endfunction()

find_symbol_offset(${TESTLIB} "add" NM_ADD_OFFSET)
find_symbol_offset(${TESTLIB} "get_message" NM_GET_MESSAGE_OFFSET)
find_symbol_offset(${TESTLIB} "global_buffer1" NM_GLOBAL_BUFFER1_OFFSET)


add_test(NAME repl_test_calculate_offset_store_increment
COMMAND cliffi --repltest
calculate_offset testlib_offset ${TESTLIB} add ${NM_ADD_OFFSET} \n
store testlib_offset+${NM_GLOBAL_BUFFER1_OFFSET} 5 \n
${TESTLIB} i increment_at_pointer -P testlib_offset+${NM_GLOBAL_BUFFER1_OFFSET} \n
)
set_tests_properties(repl_test_calculate_offset_store_increment PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 6")


add_test(NAME repl_test_calculate_offset_call_function_relative_address
COMMAND cliffi --repltest
calculate_offset ${TESTLIB} add ${NM_ADD_OFFSET} \n
${TESTLIB} i ${NM_ADD_OFFSET} 3 4 \n
)
set_tests_properties(repl_test_calculate_offset_call_function_relative_address PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 7")

add_test(NAME repl_test_calculate_offset_call_function_absolute_address
COMMAND cliffi --repltest
calculate_offset testlib_offset ${TESTLIB} add ${NM_ADD_OFFSET} \n
${TESTLIB} s testlib_offset+${NM_GET_MESSAGE_OFFSET} \n
)
set_tests_properties(repl_test_calculate_offset_call_function_absolute_address PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"?Hello, cliffi.\"?")


endif()

add_test(NAME repl_test_cast_int_to_char
COMMAND cliffi --repltest
set var1 35 \n
var2 = -c var1 \n
)
set_tests_properties(repl_test_cast_int_to_char PROPERTIES PASS_REGULAR_EXPRESSION "char var2 = \'?#\'?")

add_test(NAME repl_test_cast_char_to_int
COMMAND cliffi --repltest
set var1 -c q \n
var2 = -i var1 \n
)
set_tests_properties(repl_test_cast_char_to_int PROPERTIES PASS_REGULAR_EXPRESSION "int var2 = 113")

add_test(NAME repl_test_cast_int_to_double
COMMAND cliffi --repltest
set var1 35 \n
var2 = -d var1 \n
)
set_tests_properties(repl_test_cast_int_to_double PROPERTIES PASS_REGULAR_EXPRESSION "double var2 = 35.00*")

add_test(NAME repl_test_cast_double_to_int
COMMAND cliffi --repltest
set var1 35.5 \n
var2 = -i var1 \n
)
set_tests_properties(repl_test_cast_double_to_int PROPERTIES PASS_REGULAR_EXPRESSION "int var2 = 35")

add_test(NAME repl_test_cast_int_to_uint
COMMAND cliffi --repltest
set var1 35 \n
var2 = -I var1 \n
)
set_tests_properties(repl_test_cast_int_to_uint PROPERTIES PASS_REGULAR_EXPRESSION "uint var2 = 35")


add_test(NAME repl_test_cast_struct_to_array_and_back
COMMAND cliffi --repltest
foo = -S: 1 2.2 a hello :S \n
bar = -aC80 foo \n
#set baz -S: 0 0.0 a empty :S bar \n )
baz = NS: i d c s :S bar \n)
set_tests_properties(repl_test_cast_struct_to_array_and_back PROPERTIES PASS_REGULAR_EXPRESSION "struct baz = { int 1, double 2.20*, char \'?a\'?, cstring \"?hello")

add_test(NAME repl_test_modify_var_value_with_cast_and_store
COMMAND cliffi --repltest
set var1 -pi 35 \n
avar1 = -P var1 \n
store avar1 36 \n
var1 \n
)
set_tests_properties(repl_test_modify_var_value_with_cast_and_store PROPERTIES PASS_REGULAR_EXPRESSION "int. var1 = 36")


# add a bunch more tests for casting
# test voidpointers and arrays and structs etc


if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
add_test(NAME repl_test_calculate_returns_even
COMMAND cliffi --repltest
calculate_offset testlib_offset ${TESTLIB} add 0x10000 \n
calculate_offset testlib_offset2 ${TESTLIB} add 0x10001 \n

testlib_offset \n
testlib_offset2 \n
)
set_tests_properties(repl_test_calculate_returns_even PROPERTIES PASS_REGULAR_EXPRESSION "testlib_offset = [0-9a-fA-FxX]+[02468ace]")
set_tests_properties(repl_test_calculate_returns_even PROPERTIES PASS_REGULAR_EXPRESSION "testlib_offset2 = [0-9a-fA-FxX]+[02468ace]")
endif()


# Create setup fixture for the test


add_test(NAME repl_test_cliffi_init
  COMMAND cliffi --repltest
    print var \n
    print svar \n
)

set_tests_properties(
repl_test_cliffi_init PROPERTIES
PASS_REGULAR_EXPRESSION "int var = 4"
PASS_REGULAR_EXPRESSION "svar = \"?Hello, cliffi.\"?"
FIXTURES_REQUIRED cliffi_init_fixture
REQUIRED_FILES ${CMAKE_CURRENT_BINARY_DIR}/.cliffi_init
)

endif()

add_test(NAME setup_cliffi_init
  COMMAND sh -c "echo \"
   set var 4\n
   svar = -s placeholder\n
   ${TESTLIB} svar get_message\n
   ${TESTLIB} i increment_global\n
    \" > ${CMAKE_CURRENT_BINARY_DIR}/.cliffi_init"
)
set_tests_properties(setup_cliffi_init PROPERTIES FIXTURES_SETUP cliffi_init_fixture)

# Create cleanup fixture for the test
add_test(NAME cleanup_cliffi_init
  COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_BINARY_DIR}/.cliffi_init
)
set_tests_properties(cleanup_cliffi_init PROPERTIES FIXTURES_CLEANUP cliffi_init_fixture)


add_test(NAME test_cliffi_init_without_repl
  COMMAND cliffi ${TESTLIB} i increment_global
)

set_tests_properties(
test_cliffi_init_without_repl PROPERTIES
PASS_REGULAR_EXPRESSION "Function returned: 2" # because it was incremented once in the init
FIXTURES_REQUIRED cliffi_init_fixture
REQUIRED_FILES ${CMAKE_CURRENT_BINARY_DIR}/.cliffi_init
)

add_test(NAME test_cliffi_init_without_repl_using_var
  COMMAND cliffi ${TESTLIB} i add var var
)

set_tests_properties(
test_cliffi_init_without_repl_using_var PROPERTIES
PASS_REGULAR_EXPRESSION "Function returned: 8" # because it was incremented once in the init
FIXTURES_REQUIRED cliffi_init_fixture
REQUIRED_FILES ${CMAKE_CURRENT_BINARY_DIR}/.cliffi_init
)

# Define the test that requires the fixture
# add_test(NAME TestAddFunctionWithSH
# COMMAND sh cliffi ${TESTLIB} i add 1 2)
# set_tests_properties(TestAddFunctionWithSH PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 3")

# add_test(NAME TestAddFunctionWithSH2
# COMMAND env sh cliffi ${TESTLIB} i add 1 2)
# set_tests_properties(TestAddFunctionWithSH2 PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 3")
