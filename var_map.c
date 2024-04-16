#include "types_and_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NAME_LENGTH 32

typedef struct {
    char name[MAX_NAME_LENGTH];
    ArgInfo* value;
} mapEntry;

typedef struct {
    mapEntry* entries;
    int size;
    int capacity;
} varMap;

varMap* createVarMap(int initialCapacity) {
    varMap* map = (varMap*)calloc(1, sizeof(varMap));
    map->entries = (mapEntry*)calloc(initialCapacity, sizeof(mapEntry));
    map->size = 0;
    map->capacity = initialCapacity;
    return map;
}

void destroyVarMap(varMap* map) {
    free(map->entries);
    free(map);
}

ArgInfo* getVarWithMap(varMap* map, const char* name) {
    for (int i = 0; i < map->size; i++) {
        if (strncmp(map->entries[i].name, name, MAX_NAME_LENGTH - 1) == 0) {
            return map->entries[i].value;
        }
    }
    return NULL;
}

void setVarWithMap(varMap* map, const char* name, ArgInfo* value) {
    for (int i = 0; i < map->size; i++) {
        if (strcmp(map->entries[i].name, name) == 0) {
            map->entries[i].value = value;
            return;
        }
    }

    if (map->size == map->capacity) {
        map->capacity *= 2;
        map->entries = (mapEntry*)realloc(map->entries, sizeof(mapEntry) * map->capacity);
    }

    strncpy(map->entries[map->size].name, name, MAX_NAME_LENGTH - 1);
    map->entries[map->size].value = value;
    map->size++;
}

varMap* globalMap = NULL;

void initializeVarMap() {
    globalMap = createVarMap(8);
}

ArgInfo* getVar(const char* name) {
    if (globalMap == NULL) {
        initializeVarMap();
    }
    return getVarWithMap(globalMap, name);
}

void setVar(const char* name, ArgInfo* value) {
    if (globalMap == NULL) {
        initializeVarMap();
    }
    setVarWithMap(globalMap, name, value);
}