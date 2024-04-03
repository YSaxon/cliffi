cmake_minimum_required(VERSION 3.10)
project(cliffi VERSION 1.0 LANGUAGES C)

enable_testing()

# Set C standard
set(CMAKE_C_STANDARD 99)

set(LIBFFI_INCLUDE_DIRS /hardcoded/path/to/libffi/include)
set(LIBFFI_STATIC_LIB /hardcoded/path/to/libffi/lib/libffi.a)
#if above hardcoded path works then good, otherwise use pkg-config
if(EXISTS ${LIBFFI_STATIC_LIB} AND EXISTS ${LIBFFI_INCLUDE_DIRS})
    message("Hardcoded")
    set(LIBFFI_LIBRARIES ${LIBFFI_STATIC_LIB})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android" AND ANDROID_ABI STREQUAL "armeabi-v7a")
    message("compiling for Android")
    set(LIBFFI_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/libffi-3.4.6-android-27-armeabi-v7a/include)
    set(LIBFFI_STATIC_LIB ${CMAKE_SOURCE_DIR}/libffi-3.4.6-android-27-armeabi-v7a/lib/libffi.a)
    set(LIBFFI_LIBRARIES ${LIBFFI_STATIC_LIB})
else()
    message("Not Hardcoded")
    find_package(PkgConfig REQUIRED)
    pkg_search_module(LIBFFI libffi)
endif()


find_library(DL_LIBRARY NAMES dl)

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
add_executable(cliffi ${SOURCES})
target_link_libraries(cliffi ${LIBFFI_LIBRARIES})

if(DL_LIBRARY)
  target_link_libraries(cliffi ${DL_LIBRARY})
endif()
target_include_directories(cliffi PRIVATE ${LIBFFI_INCLUDE_DIRS})

# Compile cliffi_testlib.c into a shared library
add_library(cliffitest SHARED cliffi_testlib.c)

# Set the output name of the library to match the platform conventions
set_target_properties(cliffitest PROPERTIES OUTPUT_NAME "cliffi_test")

# Set the output directory for executables and libraries
set_target_properties(cliffi PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
set_target_properties(cliffitest PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})


#===============================================================================
add_test(NAME TestAddFunction
         COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} i add 1 2)
set_tests_properties(TestAddFunction PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 3")

add_test(NAME TestMultiply
         COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} d multiply 1.5d -d 2)
set_tests_properties(TestMultiply PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 3.0")

add_test(NAME TestConcat
         COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} s concat -s "test" "abc 123")
set_tests_properties(TestConcat PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"?testabc 123\"?")

add_test(NAME TestGetMessage
         COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} s get_message)
set_tests_properties(TestGetMessage PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"?Hello, cliffi!\"?")

add_test(NAME TestSumArray
         COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} i sum_array -ai 1,2,3 -i 3)
set_tests_properties(TestSumArray PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 6")

add_test(NAME TestIncrementAtPointer
         COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} i increment_at_pointer -pi 5)
set_tests_properties(TestIncrementAtPointer PROPERTIES PASS_REGULAR_EXPRESSION "Function returned:.* 6")

add_test(NAME TestReturnArray
         COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} ai6 get_array_of_int 3 6)
set_tests_properties(TestReturnArray PROPERTIES PASS_REGULAR_EXPRESSION "Function returned:.*\{ 3, 3, 3, 3, 3, 3 \}")

add_test(NAME TestReturnArrayDynamicallySized
         COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} ait2 get_array_of_int 3 6)
set_tests_properties(TestReturnArrayDynamicallySized PROPERTIES PASS_REGULAR_EXPRESSION "Function returned:.*\{ 3, 3, 3, 3, 3, 3 \}")

add_test(NAME TestIncrementAtPointerPointer
         COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} i increment_at_pointer_pointer -ppi 5)
set_tests_properties(TestIncrementAtPointerPointer PROPERTIES PASS_REGULAR_EXPRESSION "Function returned:.* 6")

add_test(NAME TestReturnArrayDoubles
         COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} adt1 get_array_of_doubles 5)
set_tests_properties(TestReturnArrayDoubles PROPERTIES PASS_REGULAR_EXPRESSION "Function returned:.*\{ 0.00*, 0.50*, 1.00*, 1.50*, 2.00* \}")

add_test(NAME TestConcatStringArray
         COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} s concat_str_array -as aa,bb,cc,dd 4)
set_tests_properties(TestConcatStringArray PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"?aabbccdd\"?")

add_test(NAME TestImplicitStringArray
COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} s concat_str_array aa,bb,cc,dd 4)
set_tests_properties(TestImplicitStringArray PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"?aabbccdd\"?")

add_test(NAME TestImplicitIntArray
COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} i sum_array 1,2,3 3)
set_tests_properties(TestImplicitIntArray PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 6")

add_test(NAME TestExplicitStringTypeOverridesArray
COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} s concat -s 1,2,3 -s aa,bb,cc)
set_tests_properties(TestExplicitStringTypeOverridesArray PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"?1,2,3aa,bb,cc\"?")

add_test(NAME TestStringInQuotesAllowedSpacesAndDashes
COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} s concat -s "1,2,3 -s aa,bb,cc" -s abc)
set_tests_properties(TestStringInQuotesAllowedSpacesAndDashes PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"?1,2,3 -s aa,bb,ccabc\"?")

add_test(NAME TestSumWithHex
         COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} i add -i 0x10 -i 0x10)
set_tests_properties(TestSumWithHex PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 32")

add_test(NAME TestSumImplicitNegativeNumberNotInterpretedAsFlag
         COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} i add -4 -5)
set_tests_properties(TestSumImplicitNegativeNumberNotInterpretedAsFlag PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: -9")

add_test(NAME TestConcatWithHexBuffers # aC and ac buffers should both work as strings if parsed properly
         COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} s concat -ac 0x3232656800 -aC 0x34353600)
set_tests_properties(TestConcatWithHexBuffers PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \"?22eh456\"?")

add_test(NAME TestReturningBuffer # aC and ac buffers should both work to contain strings
         COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} ac8 get_message)
set_tests_properties(TestReturningBuffer PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: .*H.*e.*l.*l.*o.*,")

add_test(NAME TestReturningBufferWithSize # aC and ac buffers should both work to contain strings
            COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} act2 buffer_as_return -act2 a,b,c,d 4)
set_tests_properties(TestReturningBufferWithSize PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: .*a.*b.*c.*d.*")

add_test(NAME TestGetX # 
            COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} i get_x -S: 5 2.2 :S )
set_tests_properties(TestGetX PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 5")

add_test(NAME TestGetXByReference # 
            COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} i get_x_from_structpointer -pS: 5 2.2 :S )
set_tests_properties(TestGetXByReference PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 5")

add_test(NAME ModifyPoint # 
            COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v modify_point -pS: 5 2.2 :S 1 .1 )
set_tests_properties(ModifyPoint PROPERTIES PASS_REGULAR_EXPRESSION "Arg 0 .* struct. \{ int 6, double 2.3.* \}")

add_test(NAME ComplexStruct # 
            COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v test_complex_struct -S: -c a 4 1.1 0x33 -S: 8 2.2 :S -i 54 5.4 -pS: 7 9.9 :S :S )
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
            COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v test_p_complex_struct -pS: -c a 4 1.1 0x33 -S: 8 2.2 :S -i 54 5.4 -pS: 7 9.9 :S :S )
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
COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v test_nested_large_struct -S: -c a -S: 5 3.3 -c b thisisastring :S 77 :S )
set_tests_properties(NestedLargerStruct PROPERTIES PASS_REGULAR_EXPRESSION "s.c: a")
set_tests_properties(NestedLargerStruct PROPERTIES PASS_REGULAR_EXPRESSION "s.s.x: 5")
set_tests_properties(NestedLargerStruct PROPERTIES PASS_REGULAR_EXPRESSION "s.s.y: 3.30*")
set_tests_properties(NestedLargerStruct PROPERTIES PASS_REGULAR_EXPRESSION "s.s.c: b")
set_tests_properties(NestedLargerStruct PROPERTIES PASS_REGULAR_EXPRESSION "s.s.s: thisisastring")
set_tests_properties(NestedLargerStruct PROPERTIES PASS_REGULAR_EXPRESSION "s.x: 77")

add_test(NAME return_struct #
COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} S: i d :S get_point 4 2.2 )
set_tests_properties(return_struct PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: .*\{ int 4, double 2.2.*")

add_test(NAME return_struct_pointer #
COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} pS: i d :S get_p_point 4 2.2 )
set_tests_properties(return_struct_pointer PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: .*\{ int 4, double 2.2.*")

add_test(NAME return_larger_struct #
COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} S: i d c s :S get_larger_struct 4 2.2 -c q thisisastring )
set_tests_properties(return_larger_struct PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: .*\{ int 4, double 2.20*, char q, cstring \"thisisastring\"")

add_test(NAME return_larger_struct_pointer #
COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} pS: i d c s :S get_p_larger_struct 4 2.2 -c q thisisastring )
set_tests_properties(return_larger_struct_pointer PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: .*\{ int 4, double 2.20*, char q, cstring \"thisisastring\"")

add_test(NAME return_nested_larger_struct #
COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} S: c S: i d c s :S i :S get_nested_larger_struct -c a 5 3.3 -c b thisisastring 77 )
set_tests_properties(return_nested_larger_struct PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: .*\{ char a, .* \{ int 5, double 3.3.*, char b, cstring \"thisisastring\" \}, int 77.*")

add_test(NAME return_p_nested_larger_struct #
COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} pS: c S: i d c s :S i :S get_p_nested_larger_struct -c a 5 3.3 -c b thisisastring 77 )
set_tests_properties(return_p_nested_larger_struct PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: .*\{ char a, .* \{ int 5, double 3.3.*, char b, cstring \"thisisastring\" \}, int 77.*")

add_test(NAME return_complex_struct #
# ComplexStruct get_complex_struct(unsigned char c, int x, double y, unsigned char c2, int x2, double y2, int x3, double y3){
COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} S: c i d C S: i d :S i d pS: i d :S :S get_complex_struct -c 1 1 1.1 -C 0x32 2 2.2 3 3.3 4 4.4 )
set_tests_properties(return_complex_struct PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \{ char 1, int 1, double 1.10*, uchar [^,]+, .*\{ int 2, double 2.20* \}, int 3, double 3.30*, .*\{ int 4, double 4.40* \}")

add_test(NAME return_p_complex_struct #
# ComplexStruct get_complex_struct(unsigned char c, int x, double y, unsigned char c2, int x2, double y2, int x3, double y3){
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} pS: c i d C S: i d :S i d pS: i d :S :S get_p_complex_struct -c 1 1 1.1 -C 0x32 2 2.2 3 3.3 4 4.4 )
set_tests_properties(return_p_complex_struct PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \{ char 1, int 1, double 1.10*, uchar [^,]+, .*\{ int 2, double 2.20* \}, int 3, double 3.30*, .*\{ int 4, double 4.40* \}")

add_test(NAME test_struct_with_embedded_array #
    # COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v test_struct_with_embedded_array -S: 1 -ac10 h,e,l,l,o 1,2,3 4 )
COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v test_struct_with_embedded_array -S: 1 -ac10 h,e,l,l,o 1,2,3 4 :S )
set_tests_properties(test_struct_with_embedded_array PROPERTIES PASS_REGULAR_EXPRESSION "s.x: 1")
set_tests_properties(test_struct_with_embedded_array PROPERTIES PASS_REGULAR_EXPRESSION "s.a[0]: 1")
set_tests_properties(test_struct_with_embedded_array PROPERTIES PASS_REGULAR_EXPRESSION "s.a[1]: 2")
set_tests_properties(test_struct_with_embedded_array PROPERTIES PASS_REGULAR_EXPRESSION "s.a[2]: 3")
set_tests_properties(test_struct_with_embedded_array PROPERTIES PASS_REGULAR_EXPRESSION "s.y: 4")
set_tests_properties(test_struct_with_embedded_array PROPERTIES PASS_REGULAR_EXPRESSION "s.s: hello")

add_test(NAME test_p_struct_with_embedded_array #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v test_p_struct_with_embedded_array -pS: 1 -ac10 h,e,l,l,o 1,2,3 4 :S )
set_tests_properties(test_p_struct_with_embedded_array PROPERTIES PASS_REGULAR_EXPRESSION "s->x: 1")
set_tests_properties(test_p_struct_with_embedded_array PROPERTIES PASS_REGULAR_EXPRESSION "s->s: hello")
set_tests_properties(test_p_struct_with_embedded_array PROPERTIES PASS_REGULAR_EXPRESSION "s->a[0]: 1")
set_tests_properties(test_p_struct_with_embedded_array PROPERTIES PASS_REGULAR_EXPRESSION "s->a[1]: 2")
set_tests_properties(test_p_struct_with_embedded_array PROPERTIES PASS_REGULAR_EXPRESSION "s->a[2]: 3")
set_tests_properties(test_p_struct_with_embedded_array PROPERTIES PASS_REGULAR_EXPRESSION "s->y: 4")

add_test(NAME test_p_struct_with_embedded_array_intonly #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v test_p_struct_with_embedded_array_intonly -pS: 1 1,2,3 4 :S )
set_tests_properties(test_p_struct_with_embedded_array_intonly PROPERTIES PASS_REGULAR_EXPRESSION "s->x: 1")
set_tests_properties(test_p_struct_with_embedded_array_intonly PROPERTIES PASS_REGULAR_EXPRESSION "s->s: hello")
set_tests_properties(test_p_struct_with_embedded_array_intonly PROPERTIES PASS_REGULAR_EXPRESSION "s->a[0]: 1")
set_tests_properties(test_p_struct_with_embedded_array_intonly PROPERTIES PASS_REGULAR_EXPRESSION "s->a[1]: 2")
set_tests_properties(test_p_struct_with_embedded_array_intonly PROPERTIES PASS_REGULAR_EXPRESSION "s->a[2]: 3")
set_tests_properties(test_p_struct_with_embedded_array_intonly PROPERTIES PASS_REGULAR_EXPRESSION "s->y: 4")
set_tests_properties(test_p_struct_with_embedded_array_intonly PROPERTIES PASS_REGULAR_EXPRESSION "Arg 0 after function .*1, 2, 3")

add_test(NAME test_p_struct_with_embedded_array_intonly_return_struct #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} S: i ai3 i :S test_p_struct_with_embedded_array_intonly_return_struct -pS: 1 1,2,3 4 :S )
set_tests_properties(test_p_struct_with_embedded_array_intonly_return_struct PROPERTIES PASS_REGULAR_EXPRESSION "s->x: 1")
set_tests_properties(test_p_struct_with_embedded_array_intonly_return_struct PROPERTIES PASS_REGULAR_EXPRESSION "s->a[0]: 1")
set_tests_properties(test_p_struct_with_embedded_array_intonly_return_struct PROPERTIES PASS_REGULAR_EXPRESSION "s->a[1]: 2")
set_tests_properties(test_p_struct_with_embedded_array_intonly_return_struct PROPERTIES PASS_REGULAR_EXPRESSION "s->a[2]: 3")
set_tests_properties(test_p_struct_with_embedded_array_intonly_return_struct PROPERTIES PASS_REGULAR_EXPRESSION "s->y: 4")

add_test(NAME return_struct_with_embedded_array_intonly #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} S: i ai3 i :S return_struct_with_embedded_array_intonly )
set_tests_properties(return_struct_with_embedded_array_intonly PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \{ int 1, .*2, 3, 4.* int 5")


add_test(NAME test_p_struct_with_embedded_array_charonly #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v test_p_struct_with_embedded_array_charonly -pS: 1 -ac10 h,e,l,l,o 4 :S )
set_tests_properties(test_p_struct_with_embedded_array_charonly PROPERTIES PASS_REGULAR_EXPRESSION "s->x: 1")
set_tests_properties(test_p_struct_with_embedded_array_charonly PROPERTIES PASS_REGULAR_EXPRESSION "s->s: hello")
set_tests_properties(test_p_struct_with_embedded_array_charonly PROPERTIES PASS_REGULAR_EXPRESSION "s->y: 4")

add_test(NAME return_simple_struct_embedded_array #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} S: ai3 :S return_simple_struct_embedded_array )
set_tests_properties(return_simple_struct_embedded_array PROPERTIES PASS_REGULAR_EXPRESSION "Function returned:.*1, 2, 3.*")


add_test(NAME test_p_struct_with_embedded_array_changed #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v test_p_struct_with_embedded_array_changed -pS: 1 -ac10 h,e,l,l,o 1,2,3 4 :S )
set_tests_properties(test_p_struct_with_embedded_array_changed PROPERTIES PASS_REGULAR_EXPRESSION "5.*changed.*10, 11, 12.*15")



add_test(NAME test_fixed_char_array_10_with_incomplete_values #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v test_fixed_buffer_10_chars_arg_then_double_arg -ac10 h,e,l,l,o 4.4 )
set_tests_properties(test_fixed_char_array_10_with_incomplete_values PROPERTIES PASS_REGULAR_EXPRESSION "s: hello")
set_tests_properties(test_fixed_char_array_10_with_incomplete_values PROPERTIES PASS_REGULAR_EXPRESSION "d: 4.40*")

add_test(NAME test_fixed_char_array_10_with_full_values_then_double #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v test_fixed_buffer_10_chars_arg_then_double_arg -ac10 h,e,l,l,o,a,b,c,d,0x00 4.4 )
set_tests_properties(test_fixed_char_array_10_with_full_values_then_double PROPERTIES PASS_REGULAR_EXPRESSION "s: helloabcd")
set_tests_properties(test_fixed_char_array_10_with_full_values_then_double PROPERTIES PASS_REGULAR_EXPRESSION "d: 4.40*")

add_test(NAME test_simple_struct_embedded_array_10_chars_filled_then_double #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v test_simple_struct_embedded_array_10_chars_then_double -S: -ac10 h,e,l,l,o,a,b,c,d,0x00 4.4 :S )
set_tests_properties(test_simple_struct_embedded_array_10_chars_filled_then_double PROPERTIES PASS_REGULAR_EXPRESSION "s: helloabcd")
set_tests_properties(test_simple_struct_embedded_array_10_chars_filled_then_double PROPERTIES PASS_REGULAR_EXPRESSION "d: 4.40*")

add_test(NAME test_simple_struct_embedded_array_10_chars_then_double #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v test_simple_struct_embedded_array_10_chars_then_double -S: -ac10 h,e,l,l,o,0x00,q 4.4 :S )
    set_tests_properties(test_simple_struct_embedded_array_10_chars_then_double PROPERTIES PASS_REGULAR_EXPRESSION "s: hello")
    set_tests_properties(test_simple_struct_embedded_array_10_chars_then_double PROPERTIES PASS_REGULAR_EXPRESSION "d: 4.40*")

add_test(NAME test_even_simpler_struct_embedded_array_10_chars_unfilled
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v test_even_simpler_struct_embedded_array_10_chars -S: -ac10 h,e,l,l,o,0x00,q :S )
set_tests_properties(test_even_simpler_struct_embedded_array_10_chars_unfilled PROPERTIES PASS_REGULAR_EXPRESSION "s: hello")

add_test(NAME test_even_simpler_struct_embedded_array_10_chars_filled
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v test_even_simpler_struct_embedded_array_10_chars -S: -ac10 h,e,l,l,o,a,b,c,d,0x00 :S )
set_tests_properties(test_even_simpler_struct_embedded_array_10_chars_filled PROPERTIES PASS_REGULAR_EXPRESSION "s: helloabcd")


add_test(NAME test_struct_with_embedded_array_pointer #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v test_struct_with_char_pointer -S: -pac10 h,e,l,l,o,0x00,q :S )
set_tests_properties(test_struct_with_embedded_array_pointer PROPERTIES PASS_REGULAR_EXPRESSION "s: hello")

add_test(NAME test_p_struct_with_embedded_array_pointer #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v test_p_struct_with_char_pointer -pS: -pac10 h,e,l,l,o,0x00,q :S )
set_tests_properties(test_p_struct_with_embedded_array_pointer PROPERTIES PASS_REGULAR_EXPRESSION "s: hello")
set_tests_properties(test_p_struct_with_embedded_array_pointer PROPERTIES PASS_REGULAR_EXPRESSION "after function return:.*hello")




add_test(NAME func_with_int_varargs #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} i sum_func_with_int_varargs 10 ... 1 2 3 4 5 6 7 8 9 10)
set_tests_properties(func_with_int_varargs PROPERTIES PASS_REGULAR_EXPRESSION "got vararg 0: 1")
set_tests_properties(func_with_int_varargs PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 55")

add_test(NAME varargs_structs #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v varargs_structs 3 ... -S: 1 2.2 :S -S: 3 4.4 :S -S: 5 6.6 :S )
set_tests_properties(varargs_structs PROPERTIES PASS_REGULAR_EXPRESSION "got vararg 0: 1, 2.2.*")
set_tests_properties(varargs_structs PROPERTIES PASS_REGULAR_EXPRESSION "got vararg 1: 3, 4.4.*")
set_tests_properties(varargs_structs PROPERTIES PASS_REGULAR_EXPRESSION "got vararg 2: 5, 6.6.*")

add_test(NAME varargs_p_structs #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v varargs_p_structs 3 ... -pS: 1 2.2 :S -pS: 3 4.4 :S -pS: 5 6.6 :S )
set_tests_properties(varargs_p_structs PROPERTIES PASS_REGULAR_EXPRESSION "got vararg 0: 1, 2.2.*")
set_tests_properties(varargs_p_structs PROPERTIES PASS_REGULAR_EXPRESSION "got vararg 1: 3, 4.4.*")
set_tests_properties(varargs_p_structs PROPERTIES PASS_REGULAR_EXPRESSION "got vararg 2: 5, 6.6.*")

add_test(NAME my_printf_novarargs #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v my_printf -s "Hello" ...)
set_tests_properties(my_printf_novarargs PROPERTIES PASS_REGULAR_EXPRESSION "Hello")

add_test(NAME my_printf_string #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v my_printf -s "Hello, %s!" ... "cliffi")
set_tests_properties(my_printf_string PROPERTIES PASS_REGULAR_EXPRESSION "Hello, cliffi!")

add_test(NAME my_printf_with_intonly #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v my_printf -s "Hello, %d" ... 7)
set_tests_properties(my_printf_with_intonly PROPERTIES PASS_REGULAR_EXPRESSION "Hello, 7")

add_test(NAME my_printf_with_int #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v my_printf -s "Hello, %s! %d" ... "cliffi" 5)
set_tests_properties(my_printf_with_int PROPERTIES PASS_REGULAR_EXPRESSION "Hello, cliffi! 5")

add_test(NAME my_printf_with_double #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v my_printf -s "Hello, %s! %f" ... "cliffi" 5.5)
set_tests_properties(my_printf_with_double PROPERTIES PASS_REGULAR_EXPRESSION "Hello, cliffi! 5.50*")

add_test(NAME my_printf_with_double_and_int #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v my_printf -s "Hello, %s! %f %d" ... "cliffi" 5.5 5)
set_tests_properties(my_printf_with_double_and_int PROPERTIES PASS_REGULAR_EXPRESSION "Hello, cliffi! 5.50* 5")

add_test(NAME my_printf_with_double_and_int_and_string #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v my_printf -s "Hello, %s! %f %d %s" ... "cliffi" 5.5 5 "cliffi")
set_tests_properties(my_printf_with_double_and_int_and_string PROPERTIES PASS_REGULAR_EXPRESSION "Hello, cliffi! 5.50* 5 cliffi")

add_test(NAME my_printf_with_double_and_int_and_string_and_int #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v my_printf -s "Hello, %s! %f %d %s %d" ... "cliffi" 5.5 5 "cliffi" 3)
set_tests_properties(my_printf_with_double_and_int_and_string_and_int PROPERTIES PASS_REGULAR_EXPRESSION "Hello, cliffi! 5.50* 5 cliffi 3")

add_test(NAME my_printf_with_char #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v my_printf -s "Hello, %c" ... -c q)
set_tests_properties(my_printf_with_char PROPERTIES PASS_REGULAR_EXPRESSION "Hello, q")

add_test(NAME my_printf_with_double_and_int_and_string_and_char #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v my_printf -s "Hello, %s! %f %d %s %c" ... "cliffi" 5.5 5 "cliffi" -c z)
set_tests_properties(my_printf_with_double_and_int_and_string_and_char PROPERTIES PASS_REGULAR_EXPRESSION "Hello, cliffi! 5.50* 5 cliffi z")

add_test(NAME my_printf_with_double_and_int_and_string_and_char_and_string #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v my_printf -s "Hello, %s! %f %d %s %c %s" ... "cliffi" 5.5 5 "cliffi" -c z "cliffi")
set_tests_properties(my_printf_with_double_and_int_and_string_and_char_and_string PROPERTIES PASS_REGULAR_EXPRESSION "Hello, cliffi! 5.50* 5 cliffi z cliffi")

add_test(NAME my_printf_with_char_and_uchar_and_short_and_ushort_and_float # test all the types that need upgrading
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v my_printf -s "Hello, %c %c %hd %hu %f" ... -c q -C 0x32 -h 3 -H 4 -f 5.5)
set_tests_properties(my_printf_with_char_and_uchar_and_short_and_ushort_and_float PROPERTIES PASS_REGULAR_EXPRESSION "Hello, q 2 3 4 5.50*")

add_test(NAME my_printf_with_p_char_and_p_uchar_and_p_short_and_p_ushort_and_p_float_and_p_int_and_p_double_and_p_struct # test with a bunch of pointers
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v my_printf -s "Hello, %p %p %p %p %p %p %p %p" ... -pc q -pC 0x32 -ph 3 -pH 4 -pf 5.5 -pi 6 -pd 7.7 -pS: 8 9.9 :S)
# can't really test the pointers, but we can test that they are printed. we're using very liberal pointer formatting regex here to allow for different pointer formats
set_tests_properties(my_printf_with_p_char_and_p_uchar_and_p_short_and_p_ushort_and_p_float_and_p_int_and_p_double_and_p_struct PROPERTIES PASS_REGULAR_EXPRESSION "Hello, 0.* 0.* 0.* 0.* 0.* 0.* 0.* 0.*")


#test that short types are okay as returns
add_test(NAME get_char #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} c get_char)
set_tests_properties(get_char PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: a")

add_test(NAME get_short #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} h get_short)
set_tests_properties(get_short PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 1")

add_test(NAME get_float #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} f get_float)
set_tests_properties(get_float PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 1.50*")

add_test(NAME get_short_negative5 #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} h get_short_negative5)
set_tests_properties(get_short_negative5 PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: -5")

add_test(NAME get_uchar_255 #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} C get_uchar_255)
set_tests_properties(get_uchar_255 PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 255")

add_test(NAME get_ushort_65535 #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} H get_ushort_65535)
set_tests_properties(get_ushort_65535 PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 65535")


add_test(NAME return_some_numbers_via_sizet_outparam #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v return_some_numbers -pait2 null -pi null)
set_tests_properties(return_some_numbers_via_sizet_outparam PROPERTIES PASS_REGULAR_EXPRESSION "\{ 1, 2, 3 \}")
add_test(NAME TestNegativeInts
COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} i add -99 -1)
set_tests_properties(TestNegativeInts PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: -100")

add_test(NAME TestLongInts
         COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} l add_long -l -5000 -l -5000)
set_tests_properties(TestLongInts PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: -10000")

add_test(NAME TestUnsignedLongs
         COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} L add_ulong -L 2147483647 -L 2147483647)
set_tests_properties(TestUnsignedLongs PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 4294967294")

add_test(NAME TestDoublePointers
COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} i increment_at_pointer_pointer -ppi 42)

add_test(NAME TestStructWithEmbeddedArray
COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v test_struct_with_embedded_array -S: 42 -ac10 h,e,l,l,o,0x00 1,2,3 99 :S)
set_tests_properties(TestStructWithEmbeddedArray PROPERTIES PASS_REGULAR_EXPRESSION "s\\.x: 42.*s\\.s:.*hello.*s\\.a\\[0\\]: 1.*s\\.a\\[1\\]: 2.*s\\.a\\[2\\]: 3.*s\\.y: 99")


add_test(NAME TestComplexVarargs
COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v my_printf -s "integers: %d %d %d, strings: %s %s, floats: %f %f, char: %c" ... -i 1 -i 2 -i 3 -s foo -s bar -f 1.23 -d 4.56 -c z)
set_tests_properties(TestComplexVarargs PROPERTIES PASS_REGULAR_EXPRESSION "integers: 1 2 3, strings: foo bar, floats: 1.23.* 4.56.*, char: z")

add_test(NAME TestUnsignedTypes
COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v my_printf -s "unsigned: %hhu %hu %u %lu" ... -C 255 -H 65535 -I 4294967295 -L 4294967295)
set_tests_properties(TestUnsignedTypes PROPERTIES PASS_REGULAR_EXPRESSION "unsigned: 255 65535 4294967295 4294967295")

add_test(NAME return_struct_with_embedded_array_complex #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} S: i ac10 ai3 i :S return_struct_with_embedded_array )
set_tests_properties(return_struct_with_embedded_array_complex PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \{ int 1,.*h.?e.?l.?l.?o.?.*2, 3, 4.*int 5")

add_test(NAME return_struct_with_embedded_array_pointer
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} S: pac10 :S return_struct_with_embedded_array_pointer)
set_tests_properties(return_struct_with_embedded_array_pointer PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: .*h.?e.?l.?l.?o.?")

add_test(NAME return_p_struct_with_embedded_array_pointer
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} pS: pac10 :S return_p_struct_with_embedded_array_pointer)
set_tests_properties(return_p_struct_with_embedded_array_pointer PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: .*h.?e.?l.?l.?o.?")

add_test(NAME grand_test_of_arrays_of_int_pointers
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} api12 grand_test_of_arrays_of_int_pointers -S: -api3 1,2,3 :S -pS: -api3 4,5,6 :S -api3 7,8,9 -papi3 10,11,12)
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
    set_tests_properties(grand_test_of_arrays_of_int_pointers PROPERTIES PASS_REGULAR_EXPRESSION "Function returned:.*1.*2.*3.*4.*5.*6.*7.*8.*9.*10.*11.*12")

add_test(NAME test_array_of_int_pointers_simple
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} i test_array_of_int_pointers_simple -api3 1,2,3)
    set_tests_properties(test_array_of_int_pointers_simple PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 6")

add_test(NAME test_array_of_pointers_in_struct
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} i test_array_of_pointers_in_struct -S: -api3 1,2,3 4,5,6 :S)
    set_tests_properties(test_array_of_pointers_in_struct PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: 6")



add_test(NAME get_packed_struct #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} SP: c i ac2 d ai3 c :S get_packed_struct )

add_test(NAME get_p_packed_struct #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} pSP: c i ac2 d ai3 c :S get_p_packed_struct )

add_test(NAME test_packed_struct #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v test_packed_struct -SP: -c a 0 a,b 4.5 1,2,3 -c q :S)
    set_tests_properties(test_packed_struct PROPERTIES PASS_REGULAR_EXPRESSION "s.c: a")
    set_tests_properties(test_packed_struct PROPERTIES PASS_REGULAR_EXPRESSION "s.i: 0")
    set_tests_properties(test_packed_struct PROPERTIES PASS_REGULAR_EXPRESSION "s.s[0]: a")
    set_tests_properties(test_packed_struct PROPERTIES PASS_REGULAR_EXPRESSION "s.s[1]: b")
    set_tests_properties(test_packed_struct PROPERTIES PASS_REGULAR_EXPRESSION "s.d: 4.50*")



add_test(NAME test_p_packed_struct #
    COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} v test_p_packed_struct -pSP: -c a 0 a,b 4.5 1,2,3 -c q :S)
    set_tests_properties(test_p_packed_struct PROPERTIES PASS_REGULAR_EXPRESSION "s->c: a")
    set_tests_properties(test_p_packed_struct PROPERTIES PASS_REGULAR_EXPRESSION "s->i: 0")
    set_tests_properties(test_p_packed_struct PROPERTIES PASS_REGULAR_EXPRESSION "s->s[0]: a")
    set_tests_properties(test_p_packed_struct PROPERTIES PASS_REGULAR_EXPRESSION "s->s[1]: b")
    set_tests_properties(test_p_packed_struct PROPERTIES PASS_REGULAR_EXPRESSION "s->d: 4.50*")
