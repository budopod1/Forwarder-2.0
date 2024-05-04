#include <stdlib.h>
#include <stdarg.h>

#ifndef PSTR_H
#define PSTR_H

struct PStr {
    // if capacity is -1: PStr is dependent, memory is managed elsewhere
    // if capacity is -2: PStr is parent of dependencies, memory is managed here
    int capacity;
    int length;
    char *text;
};

struct PStrList {
    int count;
    struct PStr *items;
};

struct PStrPair {
    struct PStr first;
    struct PStr second;
};

struct PStr *new_PStr();

void free_PStr(struct PStr *str);

void free_PStrList(struct PStrList *list);

void free_PStrPair(struct PStrPair *pair);

void print_PStr(struct PStr *str);

void null_terminate_PStr(struct PStr *str);

struct PStr *slice_PStr(struct PStr *source, int start, int len);

struct PStrList *split_PStr(struct PStr *txt, char *splitter);

int CStr_equals_PStr(char *cstr, struct PStr *pstr);

struct PStrPair *partition_PStr(struct PStr *txt, char *splitter);

struct PStr *clone_PStr(struct PStr *old);

void copy_to_PStr(struct PStr *from, struct PStr *to);

void move_PStr(struct PStr *from, struct PStr *to);

void CStr_copy_to_PStr(char *from, struct PStr *to);

void extend_PStr(struct PStr *str, const char *other, int other_len);

struct PStr *join_PStrList(struct PStrList *list, char *sep, int sep_len);

struct PStr *_build_PStr(const char *fmt, va_list args);

struct PStr *build_PStr(const char *fmt, ...);

void printf_PStr(const char *fmt, ...);

#endif