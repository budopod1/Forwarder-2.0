#include "pstr.h"
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct PStr *new_PStr() {
    return calloc(1, sizeof(struct PStr));
}

struct PStr *PStr_from_CStr(char *cstr) {
    struct PStr *str = malloc(sizeof(struct PStr));
    str->length = strlen(cstr);
    str->capacity = -1;
    str->text = cstr;
    return str;
}

char *CStr_from_PStr(struct PStr *str) {
    char *result = malloc(str->length + 1);
    memcpy(result, str->text, str->length);
    result[str->length] = '\0';
    return result;
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

struct PStr *read_file(char *path) {
    // https://stackoverflow.com/a/14002993

    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        printf("Failed to find requested file %s\n", path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *text = malloc(size);
    if (fread(text, size, 1, file) < 1) {
        printf("Failed to load requested file %s\n", path);
        return NULL;
    }
    fclose(file);

    struct PStr *result = malloc(sizeof(*result));
    result->capacity = size;
    result->length = size;
    result->text = text;

    return result;
}

struct PStr *slice_PStr(struct PStr *source, int start, int len) {
    struct PStr *str = malloc(sizeof(*str));
    str->capacity = -1;
    str->length = len;
    str->text = source->text + start;
    if (source->capacity != -1) source->capacity = -2;
    return str;
}

struct PStrList *split_PStr(struct PStr *txt, char *splitter, int splitter_len) {
    struct PStrList *list = malloc(sizeof(struct PStrList));
    int count = 0;
    int capacity = 0;
    list->items = NULL;
    int secstart = 0;
    for (int i = 0; i <= txt->length - splitter_len; i++) {
        if (memcmp(txt->text + i, splitter, splitter_len) == 0) {
            if (count == capacity) {
                // TODO: smarter growing scheme
                list->items = realloc(list->items, ++capacity * sizeof(struct PStr));
            }
            struct PStr *new_val = list->items + count++;
            new_val->capacity = -1;
            new_val->length = i - secstart;
            new_val->text = txt->text + secstart;
            i += splitter_len;
            secstart = i;
        }
    }
    if (count == capacity) {
        list->items = realloc(list->items, (capacity+1) * sizeof(struct PStr));
    }
    struct PStr *new_val = list->items + count++;
    new_val->capacity = -1;
    new_val->length = txt->length - secstart;
    new_val->text = txt->text + secstart;
    list->count = count;
    if (txt->capacity != -1)
        txt->capacity = -2;
    return list;
}

struct PStrList *split_trim_PStr(struct PStr *txt, char *splitter, int splitter_len, char *trimee, int trimee_len) {
    struct PStrList *list = malloc(sizeof(struct PStrList));
    int count = 0;
    list->items = NULL;
    int secstart = 0;
    for (int i = 0; i <= txt->length - splitter_len; i++) {
        if (memcmp(txt->text + i, splitter, splitter_len) == 0) {
            list->items = realloc(list->items, ++count * sizeof(struct PStr));
            struct PStr *new_val = list->items + count - 1;
            new_val->capacity = -1;
            new_val->length = i - secstart;
            new_val->text = txt->text + secstart;
            for (i += splitter_len; i <= txt->length - trimee_len; i += trimee_len) {
                if (memcmp(txt->text + i, trimee, trimee_len) != 0) break;
            }
            secstart = i;
        }
    }
    list->items = realloc(list->items, ++count * sizeof(struct PStr));
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

struct PStrPair *partition_PStr(struct PStr *txt, char *splitter, int splitter_len) {
    for (int i = 0; i <= txt->length - splitter_len; i++) {
        if (memcmp(txt->text + i, splitter, splitter_len) == 0) {
            if (txt->capacity != -1) txt->capacity = -2;
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
    return NULL;
}

struct PStrPair *partition_trim_PStr(struct PStr *txt, char *splitter, int splitter_len, char *trimee, int trimee_len) {
    for (int i = 0; i <= txt->length - splitter_len; i++) {
        if (memcmp(txt->text + i, splitter, splitter_len) == 0) {
            struct PStrPair *pair = malloc(sizeof(struct PStrPair));
            struct PStr *first = &pair->first;
            first->capacity = -1;
            first->length = i;
            first->text = txt->text;
            int j = i + splitter_len;
            for (; j <= txt->length - trimee_len; j += trimee_len) {
                if (memcmp(txt->text + j, trimee, trimee_len) != 0) break;
            }
            struct PStr *second = &pair->second;
            second->capacity = -1;
            second->length = txt->length - j;
            second->text = txt->text + j;
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
    free(from);
}

void CStr_copy_to_PStr(char *from, struct PStr *to) {
    to->capacity = -1;
    to->length = strlen(from);
    to->text = from;
}

void require_PStr_capacity(struct PStr *str, int req_cap) {
    if (str->capacity < req_cap) {
        int new_cap = (3 * str->length) / 2;
        if (new_cap < req_cap) new_cap = req_cap;
        str->text = realloc(str->text, new_cap);
        str->capacity = new_cap;
    }
}

void extend_PStr(struct PStr *str, const char *other, int other_len) {
    if (str->capacity < 0) {
        printf("Cannot extend dependent PStr\n");
        exit(1);
    }
    int new_len = str->length + other_len;
    require_PStr_capacity(str, new_len);
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

struct PStr *PStr_replace(struct PStr *str, char *from, int from_len, char *to, int to_len) {
    struct PStrList *list = split_PStr(str, from, from_len);
    struct PStr *result = join_PStrList(list, to, to_len);
    free_PStrList(list);
    return result;
}

void PStr_replace_inline(struct PStr *str, char *from, int from_len, char *to, int to_len) {
    for (int i = 0; i <= str->length - from_len; i++) {
        if (memcmp(str->text + i, from, from_len) == 0) {
            int new_len = str->length - from_len + to_len;
            require_PStr_capacity(str, new_len);
            char *here = str->text + i;
            memmove(here + to_len, here + from_len, str->length - i - from_len);
            memcpy(here, to, to_len);
            str->length = new_len;
            i += to_len - 1;
        }
    }
}

struct PStr *PStr_replace_once(struct PStr *str, char *from, int from_len, char *to, int to_len) {
    struct PStr *result = malloc(sizeof(struct PStr));
    for (int i = 0; i <= str->length - from_len; i++) {
        if (memcmp(str->text + i, from, from_len) == 0) {
            int new_len = str->length - from_len + to_len;
            result->length = new_len;
            result->capacity = new_len;
            result->text = malloc(new_len);
            memcpy(result->text, str->text, i);
            memcpy(result->text + i, to, to_len);
            int rest = i + from_len;
            memcpy(result->text + i + to_len, str->text + rest, str->length - rest);
            return result;
        }
    }
    memcpy(result, str, sizeof(struct PStr));
    result->capacity = -1;
    return result;
}

struct PStr *PStr_remove_once(struct PStr *str, char *removee, int removee_len, bool *did_remove) {
    for (int i = 0; i <= str->length - removee_len; i++) {
        if (memcmp(str->text + i, removee, removee_len) == 0) {
            int new_len = str->length - removee_len;
            struct PStr *result = malloc(sizeof(struct PStr));
            result->length = new_len;
            result->capacity = new_len;
            result->text = malloc(new_len);
            memcpy(result->text, str->text, i);
            int rest = i + removee_len;
            memcpy(result->text + i, str->text + rest, str->length - rest);
            if (did_remove != NULL) *did_remove = 1;
            return result;
        }
    }
    if (did_remove != NULL) *did_remove = 0;
    return clone_PStr(str);
}

void PStr_lower_to_dest(struct PStr *str, struct PStr *dest) {
    char *text = malloc(str->length);
    for (int i = 0; i < str->length; i++) {
        char chr = str->text[i];
        if (chr >= 65 && chr <= 90) {
            chr |= 32;
        }
        text[i] = chr;
    }
    dest->capacity = str->length;
    dest->length = str->length;
    dest->text = text;
}

struct PStr *PStr_to_lower(struct PStr *str) {
    struct PStr *result = malloc(sizeof(struct PStr));
    PStr_lower_to_dest(str, result);
    return result;
}

int PStr_starts_with(struct PStr *str, char *sub, int sublen) {
    if (str->length < sublen) return 0;
    return memcmp(str->text, sub, sublen) == 0;
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
                struct PStr *joined = join_PStrList(list, seperator, strlen(seperator));
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
                extend_PStr(result, fmt + i, 1);
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

int CStr_parse_int(char *txt, int base, int *result) {
    char *endptr;
    *result = (int)strtol(txt, &endptr, base);
    return txt + strlen(txt) != endptr;
}

int PStr_parse_int(struct PStr *str, int base, int *result) {
    char *cstr = CStr_from_PStr(str);
    return CStr_parse_int(cstr, base, result);
}

int int_pow(int base, int exponent) {
    int result = 1;
    while (exponent) {
        if (exponent % 2) {
            result *= base;
            exponent--;
        } else {
            base *= base;
            exponent /= 2;
        }
    }
    return result;
}

struct PStr *PStr_from_int_len(int i, int digits) {
    if (int_pow(10, digits) < i) {
        printf("Not enough digits (only %d) allocated for stringification of %d\n", digits, i);
        exit(1);
    }
    struct PStr *result = malloc(sizeof(struct PStr));
    result->capacity = digits + 1;
    result->length = digits;
    result->text = malloc(digits + 1);
    sprintf(result->text, "%d", i);
    return result;
}

char *CStr_from_int(int i) {
    int bytes;
    if (i > 0) {
        bytes = 2 + (int)floor(log(i));
    } else if (i < 0) {
        bytes = 3 + (int)floor(log(-i));
    } else {
        bytes = 2;
    }
    char *buffer = malloc(bytes);
    sprintf(buffer, "%d", i);
    return buffer;
}

int MULTIPLIERS[] = {2, 3, 5, 7, 11, 13, 17};
int MULTIPLIER_COUNT = 7;

unsigned int dumb_hash(char *txt, int len) {
    unsigned int total = 0;
    for (int i = 0; i < len; i++) {
        total += txt[i] * MULTIPLIERS[i % MULTIPLIER_COUNT];
    }
    return total;
}
