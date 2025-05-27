#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include "types_and_utils.h"

// Declare the function to test
ArgType infer_arg_type_single(const char* argval);

void setUp(void) {}
void tearDown(void) {}

void test_infer_arg_type_single_int(void) {
    TEST_ASSERT_EQUAL_INT(TYPE_INT, infer_arg_type_single("42"));
    TEST_ASSERT_EQUAL_INT(TYPE_INT, infer_arg_type_single("-17"));
}

void test_infer_arg_type_single_float(void) {
    TEST_ASSERT_EQUAL_INT(TYPE_DOUBLE, infer_arg_type_single("3.14"));
    TEST_ASSERT_EQUAL_INT(TYPE_DOUBLE, infer_arg_type_single("-2.71"));
    TEST_ASSERT_EQUAL_INT(TYPE_FLOAT, infer_arg_type_single("2.5f"));
    TEST_ASSERT_EQUAL_INT(TYPE_DOUBLE, infer_arg_type_single("2.5d"));
}

void test_infer_arg_type_single_hex(void) {
    TEST_ASSERT_EQUAL_INT(TYPE_UCHAR, infer_arg_type_single("0x2A"));
    TEST_ASSERT_EQUAL_INT(TYPE_VOIDPOINTER, infer_arg_type_single("0x12345678"));
}

void test_infer_arg_type_single_bool(void) {
    TEST_ASSERT_EQUAL_INT(TYPE_BOOL, infer_arg_type_single("true"));
    TEST_ASSERT_EQUAL_INT(TYPE_BOOL, infer_arg_type_single("false"));
    TEST_ASSERT_EQUAL_INT(TYPE_BOOL, infer_arg_type_single("TRUE"));
    TEST_ASSERT_EQUAL_INT(TYPE_BOOL, infer_arg_type_single("False"));
}

void test_infer_arg_type_single_string(void) {
    TEST_ASSERT_EQUAL_INT(TYPE_STRING, infer_arg_type_single("hello"));
    TEST_ASSERT_EQUAL_INT(TYPE_STRING, infer_arg_type_single("123abc"));
    TEST_ASSERT_EQUAL_INT(TYPE_STRING, infer_arg_type_single(""));
}

void test_infer_arg_type_single_char(void) {
    TEST_ASSERT_EQUAL_INT(TYPE_CHAR, infer_arg_type_single("a"));
    TEST_ASSERT_EQUAL_INT(TYPE_CHAR, infer_arg_type_single("Z"));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_infer_arg_type_single_int);
    RUN_TEST(test_infer_arg_type_single_float);
    RUN_TEST(test_infer_arg_type_single_hex);
    RUN_TEST(test_infer_arg_type_single_bool);
    RUN_TEST(test_infer_arg_type_single_string);
    RUN_TEST(test_infer_arg_type_single_char);
    return UNITY_END();
} 