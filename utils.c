#include <string.h>
#include "pstr.h"

int count_trailing_zeros(int num) {
    int count = 0;
    while (num > 0) {
        count++;
        num /= 2;
    }
    return count;
}

int parse_enum(struct PStr *str, char *options[], int max) {
    for (int i = 0; i < max; i++) {
        if (CStr_equals_PStr(options[i], str)) {
            return i;
        }
    }
    return max;
}

int parse_enum_flag(struct PStr *str, char *options[], int max) {
    int count = count_trailing_zeros(max);
    for (int i = 0; i < count; i++) {
        if (CStr_equals_PStr(options[i], str)) {
            return 1 << (i-1);
        }
    }
    return max;
}

bool parse_int(char *str, int strlen, int base, int *result) {
    char *newstr = malloc(strlen + 1);
    memcpy(newstr, str, strlen);
    newstr[strlen] = '\0';
    bool status = CStr_parse_int(newstr, base, result);
    free(newstr);
    return status;
}
