#include "types_and_utils.h"
#include <stdio.h>

void format_and_print_return_value(const ArgInfo* return_value){
    if (!return_value) {
        printf("No return value to display.\n");
        return;
    }
    else {
    printf("");
    }

    switch (return_value->type) {
        case TYPE_INT:
            printf("Function returned: %d\n", return_value->value.i_val);
            break;
        case TYPE_FLOAT:
            printf("Function returned: %f\n", return_value->value.f_val);
            break;
        case TYPE_DOUBLE:
            printf("Function returned: %lf\n", return_value->value.d_val);
            break;
        case TYPE_CHAR:
            printf("Function returned: '%c'\n", return_value->value.c_val);
            break;
        case TYPE_LONG:
            printf("Function returned: %ld\n", return_value->value.l_val);
            break;
        case TYPE_UCHAR:
            printf("Function returned: %u\n", (unsigned) return_value->value.uc_val); // Cast to unsigned to match %u
            break;
        case TYPE_USHORT:
            printf("Function returned: %hu\n", return_value->value.us_val);
            break;
        case TYPE_UINT:
            printf("Function returned: %u\n", return_value->value.ui_val);
            break;
        case TYPE_ULONG:
            printf("Function returned: %lu\n", return_value->value.ul_val);
            break;
        case TYPE_STRING: // Assuming the function returns a char pointer
            printf("Function returned: \"%s\"\n", return_value->value.str_val);
            break;
        case TYPE_VOID:
            printf("(Function was void)\n");
            break;
        // case TYPE_POINTER:
        //     printf("Function returned pointer: %p\n", return_value->value.ptr_val);
        //     break;
        default:
            printf("Unsupported return type.\n");
            break;
    }
}
