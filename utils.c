#include "pstr.h"

int parse_enum(struct PStr *str, char *options[], int max) {
    for (int i = 0; i < max; i++) {
        if (CStr_equals_PStr(options[i], str)) {
            return i;
        }
    }
    return max;
}
