#include "pstr.h"

#ifndef UTILS_H
#define UTILS_H

int count_trailing_zeros(int num);

int parse_enum(struct PStr *str, char *options[], int max);

int parse_enum_flag(struct PStr *str, char *options[], int max);

int parse_int(char *str, int strlen, int base, int *result);

#endif
