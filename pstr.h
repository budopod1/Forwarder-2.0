#include <stdbool.h>
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

struct PStr *PStr_from_CStr(char *cstr);

char *CStr_from_PStr(struct PStr *str);

void free_PStr(struct PStr *str);

void free_PStrList(struct PStrList *list);

void free_PStrPair(struct PStrPair *pair);

void print_PStr(struct PStr *str);

void null_terminate_PStr(struct PStr *str);

struct PStr *read_file(char *path);

struct PStr *slice_PStr(struct PStr *source, int start, int len);

struct PStrList *split_PStr(struct PStr *txt, char *splitter, int splitter_len);

struct PStrList *split_trim_PStr(struct PStr *txt, char *splitter, int splitter_len, char *trimee, int trimee_len);

int CStr_equals_PStr(char *cstr, struct PStr *pstr);

struct PStrPair *partition_PStr(struct PStr *txt, char *splitter, int splitter_len);

struct PStrPair *partition_trim_PStr(struct PStr *txt, char *splitter, int splitter_len, char *trimee, int trimee_len);

struct PStr *clone_PStr(struct PStr *old);

void copy_to_PStr(struct PStr *from, struct PStr *to);

void move_PStr(struct PStr *from, struct PStr *to);

void CStr_copy_to_PStr(char *from, struct PStr *to);

void extend_PStr(struct PStr *str, const char *other, int other_len);

struct PStr *join_PStrList(struct PStrList *list, char *sep, int sep_len);

struct PStr *PStr_replace(struct PStr *str, char *from, int from_len, char *to, int to_len);

void PStr_replace_inline(struct PStr *str, char *from, int from_len, char *to, int to_len);

struct PStr *PStr_replace_once(struct PStr *str, char *from, int from_len, char *to, int to_len);

struct PStr *PStr_remove_once(struct PStr *str, char *removee, int removee_len, bool *did_remove);

struct PStr *PStr_to_lower(struct PStr *str);

int PStr_starts_with(struct PStr *str, char *sub, int sublen);

struct PStr *_build_PStr(const char *fmt, va_list args);

struct PStr *build_PStr(const char *fmt, ...);

void printf_PStr(const char *fmt, ...);

int CStr_parse_int(char *txt, int base, int *result);

int PStr_parse_int(struct PStr *str, int base, int *result);

struct PStr *PStr_from_int_len(int i, int digits);

char *CStr_from_int(int i);

unsigned int dumb_hash(char *txt, int len);

typedef int (*recv_PStr)(struct PStr *str);

#endif
