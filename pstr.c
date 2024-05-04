#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "pstr.h"

struct PStr *new_PStr() {
    struct PStr *str = malloc(sizeof(struct PStr));
    memset(str, 0, sizeof(struct PStr));
    return str;
}

void free_PStr(struct PStr *str) {
    if (str == NULL) return;
    if (str->capacity != -1) {
        free(str->text);
    }
    free(str);
}

void free_PStrList(struct PStrList *list) {
    if (list == NULL) return;
    for (int i = 0; i < list->count; i++) {
        struct PStr *str = list->items + i;
        if (str->capacity != -1) {
            free(str->text);
        }
    }
    free(list->items);
    free(list);
}

void free_PStrPair(struct PStrPair *pair) {
    if (pair == NULL) return;
    if (pair->first.capacity != -1) {
        free(pair->first.text);
    }
    if (pair->second.capacity != -1) {
        free(pair->second.text);
    }
    free(pair);
}

void print_PStr(struct PStr *str) {
    fwrite(str->text, 1, str->length, stdout);
}

void null_terminate_PStr(struct PStr *str) {
    if (str->capacity < 0) {
        printf("Cannot null terminate dependent PStr\n");
        exit(1);
    }
    if (str->capacity == str->length) {
        str->text = realloc(str->text, str->capacity++);
    }
    str->text[str->length] = '\0';
}

struct PStr *slice_PStr(struct PStr *source, int start, int len) {
    struct PStr *str = malloc(sizeof(struct PStr));
    str->capacity = -1;
    str->length = len;
    str->text = source->text + start;
    if (source->capacity != -1) source->capacity = -2;
    return str;
}

struct PStrList *split_PStr(struct PStr *txt, char *splitter) {
    int splitter_len = strlen(splitter);
    struct PStrList *list = malloc(sizeof(struct PStrList));
    int count = 0;
    list->items = NULL;
    int secstart = 0;
    for (int i = 0; i <= txt->length - splitter_len; i++) {
        if (memcmp(txt->text + i, splitter, splitter_len) == 0) {
            list->items = realloc(list->items, ++count*sizeof(struct PStr));
            struct PStr *new_val = list->items + count - 1;
            new_val->capacity = -1;
            new_val->length = i - secstart;
            new_val->text = txt->text + secstart;
            i += splitter_len;
            secstart = i;
        }
    }
    list->items = realloc(list->items, ++count*sizeof(struct PStr));
    struct PStr *new_val = list->items + count - 1;
    new_val->capacity = -1;
    new_val->length = txt->length - secstart;
    new_val->text = txt->text + secstart;
    list->count = count;
    if (txt->capacity != -1) txt->capacity = -2;
    return list;
}

int CStr_equals_PStr(char *cstr, struct PStr *pstr) {
    if ((int)strlen(cstr) != pstr->length) return 0;
    return memcmp(cstr, pstr->text, pstr->length) == 0;
}

struct PStrPair *partition_PStr(struct PStr *txt, char *splitter) {
    int splitter_len = strlen(splitter);
    for (int i = 0; i <= txt->length - splitter_len; i++) {
        if (memcmp(txt->text + i, splitter, splitter_len) == 0) {
            struct PStrPair *pair = malloc(sizeof(struct PStrPair));
            struct PStr *first = &pair->first;
            first->capacity = -1;
            first->length = i;
            first->text = txt->text;
            struct PStr *second = &pair->second;
            second->capacity = -1;
            second->length = txt->length - i - splitter_len;
            second->text = txt->text + i + splitter_len;
            return pair;
        }
    }
    if (txt->capacity != -1) txt->capacity = -2;
    return NULL;
}

struct PStr *clone_PStr(struct PStr *old) {
    struct PStr *result = malloc(sizeof(struct PStr));
    result->capacity = old->length;
    result->length = old->length;
    result->text = malloc(old->length);
    memcpy(result->text, old->text, old->length);
    return result;
}

void copy_to_PStr(struct PStr *from, struct PStr *to) {
    to->capacity = -1;
    to->length = from->length;
    to->text = from->text;
}

void move_PStr(struct PStr *from, struct PStr *to) {
    to->capacity = from->capacity;
    to->length = from->length;
    to->text = from->text;
    from->capacity = -1;
    from->length = 0;
    from->text = NULL;
}

void CStr_copy_to_PStr(char *from, struct PStr *to) {
    to->capacity = -1;
    to->length = strlen(from);
    to->text = from;
}

void extend_PStr(struct PStr *str, const char *other, int other_len) {
    if (str->capacity < 0) {
        printf("Cannot extend dependent PStr\n");
        exit(1);
    }
    int new_len = str->length + other_len;
    if (str->capacity < new_len) {
        str->text = realloc(str->text, new_len);
        str->capacity = new_len;
    }
    memcpy(str->text + str->length, other, other_len);
    str->length = new_len;
}

struct PStr *join_PStrList(struct PStrList *list, char *sep, int sep_len) {
    if (list->count == 0) return new_PStr();
    int length = (list->count - 1) * sep_len;
    for (int i = 0; i < list->count; i++) {
        length += (list->items + i)->length;
    }
    struct PStr *str = malloc(sizeof(struct PStr));
    str->capacity = length;
    str->length = length;
    str->text = malloc(length);
    int pos = 0;
    for (int i = 0; i < list->count; i++) {
        if (i > 0) {
            memcpy(str->text + pos, sep, sep_len);
            pos += sep_len;
        }
        struct PStr *item = (list->items + i);
        memcpy(str->text + pos, item->text, item->length);
        pos += item->length;
    }
    return str;
}

struct PStr *_build_PStr(const char *fmt, va_list args) {
    struct PStr *result = new_PStr();

    int was_percent = 0;

    for (int i = 0; fmt[i]; i++) {
        if (was_percent) {
            was_percent = 0;
            if (fmt[i] == 's') {
                char *other = va_arg(args, char*);
                extend_PStr(result, other, strlen(other));
            } else if (fmt[i] == 'p') {
                struct PStr *other = va_arg(args, struct PStr*);
                extend_PStr(result, other->text, other->length);
            } else if (fmt[i] == 'l') {
                struct PStrList *list = va_arg(args, struct PStrList*);
                char *seperator = va_arg(args, char*);
                struct PStr *joined = join_PStrList(
                    list, seperator, strlen(seperator)
                );
                extend_PStr(result, joined->text, joined->length);
                free_PStr(joined);
            } else if (fmt[i] == '%') {
                extend_PStr(result, "%", 1);
            } else {
                printf("Invalid format specifier %%%c\n", fmt[i]);
                exit(1);
            }
            continue;
        } else {
            if (fmt[i] == '%') {
                was_percent = 1;
            } else {
                extend_PStr(result, fmt+i, 1);
            }
        }
    }

    return result;
}

struct PStr *build_PStr(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    struct PStr *result = _build_PStr(fmt, args);

    va_end(args);

    return result;
}

void printf_PStr(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    struct PStr *str = _build_PStr(fmt, args);

    va_end(args);

    print_PStr(str);
    free_PStr(str);
}
