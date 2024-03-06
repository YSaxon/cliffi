cmake_minimum_required(VERSION 3.10)
project(ffitool VERSION 1.0 LANGUAGES C)

enable_testing()

# Set C standard
set(CMAKE_C_STANDARD 99)

if(CMAKE_SYSTEM_NAME STREQUAL "Android" AND ANDROID_ABI STREQUAL "armeabi-v7a")
message("compiling for Android")
    set(LIBFFI_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/libffi-3.4.6-android-27-armeabi-v7a/include)
# set it to static compile against prebuilt libffi instead of shared
    set(LIBFFI_STATIC_LIB ${CMAKE_SOURCE_DIR}/libffi-3.4.6-android-27-armeabi-v7a/lib/libffi.a)
    set(LIBFFI_LIBRARIES ${LIBFFI_STATIC_LIB})
else()
    message("Not Android")
    find_package(PkgConfig REQUIRED)
    pkg_search_module(LIBFFI REQUIRED libffi)
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
set_tests_properties(TestConcat PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"?testabc 123\"?")

add_test(NAME TestGetMessage
         COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} s get_message)
set_tests_properties(TestGetMessage PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"?Hello, ffitool!\"?")

add_test(NAME TestSumArray
         COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} i sum_array -ai 1,2,3 -i 3)
set_tests_properties(TestSumArray PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 6")

add_test(NAME TestIncrementAtPointer
         COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} i increment_at_pointer -pi 5)
set_tests_properties(TestIncrementAtPointer PROPERTIES PASS_REGULAR_EXPRESSION "Function returned:.* 6")

add_test(NAME TestReturnArray
         COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} ai6 get_array_of_int 3 6)
set_tests_properties(TestReturnArray PROPERTIES PASS_REGULAR_EXPRESSION "Function returned:.*\{ 3, 3, 3, 3, 3, 3 \}")

add_test(NAME TestReturnArrayDynamicallySized
         COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} ait2 get_array_of_int 3 6)
set_tests_properties(TestReturnArrayDynamicallySized PROPERTIES PASS_REGULAR_EXPRESSION "Function returned:.*\{ 3, 3, 3, 3, 3, 3 \}")

add_test(NAME TestIncrementAtPointerPointer
         COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} i increment_at_pointer_pointer -ppi 5)
set_tests_properties(TestIncrementAtPointerPointer PROPERTIES PASS_REGULAR_EXPRESSION "Function returned:.* 6")

add_test(NAME TestReturnArrayDoubles
         COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} adt1 get_array_of_doubles 5)
set_tests_properties(TestReturnArrayDoubles PROPERTIES PASS_REGULAR_EXPRESSION "Function returned:.*\{ 0.00*, 0.50*, 1.00*, 1.50*, 2.00* \}")

add_test(NAME TestConcatStringArray
         COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} s concat_str_array -as aa,bb,cc,dd 4)
set_tests_properties(TestConcatStringArray PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"?aabbccdd\"?")

add_test(NAME TestImplicitStringArray
COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} s concat_str_array aa,bb,cc,dd 4)
set_tests_properties(TestImplicitStringArray PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"?aabbccdd\"?")

add_test(NAME TestImplicitIntArray
COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} i sum_array 1,2,3 3)
set_tests_properties(TestImplicitIntArray PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 6")

add_test(NAME TestExplicitStringTypeOverridesArray
COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} s concat -s 1,2,3 -s aa,bb,cc)
set_tests_properties(TestExplicitStringTypeOverridesArray PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"?1,2,3aa,bb,cc\"?")

add_test(NAME TestStringInQuotesAllowedSpacesAndDashes
COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} s concat -s "1,2,3 -s aa,bb,cc" -s abc)
set_tests_properties(TestStringInQuotesAllowedSpacesAndDashes PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"?1,2,3 -s aa,bb,ccabc\"?")

add_test(NAME TestSumWithHex
         COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} i add -i 0x10 -i 0x10)
set_tests_properties(TestSumWithHex PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 32")

add_test(NAME TestSumImplicitNegativeNumberNotInterpretedAsFlag
         COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} i add -4 -5)
set_tests_properties(TestSumImplicitNegativeNumberNotInterpretedAsFlag PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: -9")

add_test(NAME TestConcatWithHexBuffers # aC and ac buffers should both work as strings if parsed properly
         COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} s concat -ac 0x3232656800 -aC 0x34353600)
set_tests_properties(TestConcatWithHexBuffers PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"?22eh456\"?")

add_test(NAME TestReturningBuffer # aC and ac buffers should both work to contain strings
         COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} ac8 get_message)
set_tests_properties(TestReturningBuffer PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: .*H.*e.*l.*l.*o.*,")

add_test(NAME TestReturningBufferWithSize # aC and ac buffers should both work to contain strings
            COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} act2 buffer_as_return -act2 a,b,c,d 4)

add_test(NAME TestGetX # 
            COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} i get_x -S: 5 2.2 :S )
set_tests_properties(TestGetX PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 5")

add_test(NAME TestGetXByReference # 
            COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} i get_x_from_structpointer -pS: 5 2.2 :S )
set_tests_properties(TestGetXByReference PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 5")

add_test(NAME ModifyPoint # 
            COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} v modify_point -pS: 5 2.2 :S 1 .1 )
set_tests_properties(ModifyPoint PROPERTIES PASS_REGULAR_EXPRESSION "Arg 0 .* struct. \{ int 6, double 2.3.* \}")

add_test(NAME ComplexStruct # 
            COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} v test_complex_struct -S: -c a 4 1.1 0x33 -S: 8 2.2 :S -i 54 5.4 -pS: 7 9.9 :S :S )
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
            COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} v test_p_complex_struct -pS: -c a 4 1.1 0x33 -S: 8 2.2 :S -i 54 5.4 -pS: 7 9.9 :S :S )
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
COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} v test_nested_large_struct -S: -c a -S: 5 3.3 -c b thisisastring :S 77 :S )
set_tests_properties(NestedLargerStruct PROPERTIES PASS_REGULAR_EXPRESSION "s.c: a")
set_tests_properties(NestedLargerStruct PROPERTIES PASS_REGULAR_EXPRESSION "s.s.x: 5")
set_tests_properties(NestedLargerStruct PROPERTIES PASS_REGULAR_EXPRESSION "s.s.y: 3.30*")
set_tests_properties(NestedLargerStruct PROPERTIES PASS_REGULAR_EXPRESSION "s.s.c: b")
set_tests_properties(NestedLargerStruct PROPERTIES PASS_REGULAR_EXPRESSION "s.s.s: thisisastring")
set_tests_properties(NestedLargerStruct PROPERTIES PASS_REGULAR_EXPRESSION "s.x: 77")

add_test(NAME return_struct #
COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} S: i d :S get_point 4 2.2 )
set_tests_properties(return_struct PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: .*\{ int 4, double 2.2.*")

add_test(NAME return_struct_pointer #
COMMAND ffitool ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ffitool_test${CMAKE_SHARED_LIBRARY_SUFFIX} pS: i d :S get_p_point 4 2.2 )
set_tests_properties(return_struct_pointer PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: .*\{ int 4, double 2.2.*")