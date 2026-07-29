#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <threads.h>
#include "cache.h"

struct Cache {
    int item_capacity;
    int item_count;
    char **keys;
    void **values;
};

thread_local struct Cache cache = {0, 0, NULL, NULL};

int key_insert_position(char *key, bool *found_key) {
    int lower = 0;
    int higher = cache.item_count;
    
    while (lower != higher) {
        int idx = (lower + higher) / 2;
        char *here = cache.keys[idx];
        int comp = strcmp(key, here);
        
        if (comp < 0) {
            higher = idx;
        } else if (comp > 0) {
            lower = idx + 1;
        } else {
            *found_key = true;
            return idx;
        }
    }

    *found_key = false;
    return lower;
}

void add_to_cache(char *key, void *value) {
    bool found_key;
    int insert_pos = key_insert_position(key, &found_key);
    if (found_key) return;
    
    if (cache.item_count == cache.item_capacity) {
        int new_cap = (cache.item_capacity * 3) / 2 + 1;
        cache.item_capacity = new_cap;
        cache.keys = realloc(cache.keys, new_cap * sizeof(char*));
        cache.values = realloc(cache.values, new_cap * sizeof(void*));
    }
    
    memmove(
        cache.keys + insert_pos + 1, 
        cache.keys + insert_pos, 
        (cache.item_count-insert_pos) * sizeof(char*)
    );

    memmove(
        cache.values + insert_pos + 1, 
        cache.values + insert_pos, 
        (cache.item_count-insert_pos) * sizeof(void*)
    );

    cache.keys[insert_pos] = key;
    cache.values[insert_pos] = value;

    cache.item_count++;
}

void *get_from_cache(char *key, bool *found) {
    int idx = key_insert_position(key, found);
    if (!*found) return NULL;
    return cache.values[idx];
}
