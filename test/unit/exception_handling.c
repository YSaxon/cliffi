#include "unity.h"
#include <stdio.h>
#include <stdlib.h>

#include "exception_handling.h"
#include "shims.h"

void setUp(void) {
    main_method_install_exception_handlers();
}
void tearDown(void) {
    // Ensure that after each test, the exception state is clean.
    TEST_ASSERT_NULL(current_exception_message);
    TEST_ASSERT_NULL(current_stacktrace_strings);
    TEST_ASSERT_EQUAL_INT(0, current_stacktrace_size);
}

void test_single_exception_memory_cleanup(void) {
    // Sanity check: ensure globals are null before the test
    TEST_ASSERT_NULL(current_exception_message);
    TEST_ASSERT_NULL(current_stacktrace_strings);

    TRY {
        raiseException(1, "Test exception");
    } CATCHALL {
        // We expect to land here
        TEST_ASSERT_NOT_NULL(current_exception_message);
        TEST_ASSERT_EQUAL_STRING("Test exception\n", current_exception_message);
    } END_TRY;

    // The most important check: END_TRY should have cleaned everything up.
    TEST_ASSERT_NULL(current_exception_message);
    TEST_ASSERT_NULL(current_stacktrace_strings);
}

void test_nested_exception_memory_cleanup(void) {
    TRY {
        TRY {
            raiseException(1, "Inner exception");
        } CATCHALL {
            // We are now handling the inner exception.
            // The message and trace for "Inner exception" should exist.
            TEST_ASSERT_NOT_NULL(current_exception_message);

            // Now, raise another exception from within the catch block.
            raiseException(2, "Outer exception");
        } END_TRY;

        // This part of the outer TRY block should be skipped.
        TEST_FAIL_MESSAGE("Should not have reached here.");

    } CATCHALL {
        // We should land in the outer catch block.
        // The message should now be for the "Outer exception".
        TEST_ASSERT_EQUAL_STRING("Outer exception\n", current_exception_message);

        // In the non-backtrace case, the old message should be archived.
        // You can add more specific checks here based on the implementation.
        TEST_ASSERT_NOT_NULL(current_stacktrace_strings);

    } END_TRY;

    // After all is said and done, everything must be clean.
    TEST_ASSERT_NULL(current_exception_message);
    TEST_ASSERT_NULL(current_stacktrace_strings);
}

void test_no_exception_path(void) {
    int x = 0;
    TRY {
        x = 10;
    } CATCHALL {
        TEST_FAIL_MESSAGE("Catch block should not be entered.");
    } END_TRY;

    // Verify code was executed and memory is clean.
    TEST_ASSERT_EQUAL_INT(10, x);
    TEST_ASSERT_NULL(current_exception_message);
    TEST_ASSERT_NULL(current_stacktrace_strings);
}

void test_exception_path(void) {
    bool entered_catch = false;
    TRY
        // This should raise an exception.
        raiseException(1, "Test exception in TRY block");
    CATCHALL
        // We should land here.
        entered_catch = true;
        TEST_ASSERT_NOT_NULL(current_exception_message);
        TEST_ASSERT_EQUAL_STRING("Test exception in TRY block\n", current_exception_message);
    END_TRY;

    // After handling the exception, everything should be cleaned up.
    TEST_ASSERT_TRUE(entered_catch);
    TEST_ASSERT_NULL(current_exception_message);
    TEST_ASSERT_NULL(current_stacktrace_strings);
}

void test_nested_try_single_exception(void) {
    bool entered_inner_catch = false;
    TRY
        TRY
            raiseException(1, "Nested exception");
        CATCHALL
            // We should land here for the nested exception.
            TEST_ASSERT_NOT_NULL(current_exception_message);
            TEST_ASSERT_EQUAL_STRING("Nested exception\n", current_exception_message);
            entered_inner_catch = true;
        END_TRY;
    CATCHALL
        // This should not be reached.
        TEST_FAIL_MESSAGE("Outer catch block should not be entered.");
    END_TRY;

    // After handling the nested exception, everything should be cleaned up.
    TEST_ASSERT_TRUE(entered_inner_catch);
    TEST_ASSERT_NULL(current_exception_message);
    TEST_ASSERT_NULL(current_stacktrace_strings);
}

void test_exception_in_catch_block(void) {
    TRY
        TRY
            raiseException(1, "Exception in inner TRY");
        CATCHALL
            // We should land here.
            TEST_ASSERT_NOT_NULL(current_exception_message);
            TEST_ASSERT_EQUAL_STRING("Exception in inner TRY\n", current_exception_message);

            // Now raise another exception.
            raiseException(2, "Exception in CATCH block");
        END_TRY;
        TEST_FAIL_MESSAGE("Should not reach here after raising an exception in CATCH block.");
    CATCHALL
        // We should land here for the second exception.
        TEST_ASSERT_NOT_NULL(current_exception_message);
        TEST_ASSERT_EQUAL_STRING("Exception in CATCH block\n", current_exception_message);
    END_TRY;

    // After handling both exceptions, everything should be cleaned up.
    TEST_ASSERT_NULL(current_exception_message);
    TEST_ASSERT_NULL(current_stacktrace_strings);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_single_exception_memory_cleanup);
    RUN_TEST(test_nested_exception_memory_cleanup);
    RUN_TEST(test_no_exception_path);
    return UNITY_END();
}