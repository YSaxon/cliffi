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


# these still fail
# add_test(NAME return_struct_with_embedded_array_complex #
#     COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} S: i ai3 ac10 i :S return_struct_with_embedded_array )
# set_tests_properties(return_struct_with_embedded_array_complex PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: \{ int 1, int \{ 1, 2, 3 \}, .*h.?e.?l.?l.?o.?, int 5")

# add_test(NAME return_struct_with_embedded_array_pointer
#     COMMAND cliffi ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cliffi_test${CMAKE_SHARED_LIBRARY_SUFFIX} S: ac10 :S return_struct_with_embedded_array)
# set_tests_properties(return_struct_with_embedded_array_pointer PROPERTIES PASS_REGULAR_EXPRESSION "Function returned: .*h.?e.?l.?l.?o.?")
