#ifndef CACHE_H
#define CACHE_H

void add_to_cache(char *key, void *value);

void *get_from_cache(char *key, bool *found);

#endif
