#include "darray.h"

#include <stdio.h>
#include <string.h>

int init_dyn_array(dyn_array *arr, size_t size) {
    arr->len = 0;
    arr->array = malloc(size);
    if (arr->array == NULL) {
        arr->size = 0;
        perror("[ERROR] Failed to malloc");
        return -1;
    }
    arr->size = size;
    return 0;
}

int realloc_dyn_array(dyn_array *arr) {
    size_t new_size = arr->size * 2;
    char *tmp = realloc(arr->array, new_size);
    if (tmp == NULL) {
        perror("[ERROR] Failed to realloc");
        return -1;
    }
    arr->array = tmp;
    arr->size = new_size;
    return 0;
}

void free_dyn_array(dyn_array arr) { free(arr.array); }

int concat_to_array(dyn_array *arr, const void *data, size_t data_size) {
    while (arr->len + data_size >= arr->size) {
        if (realloc_dyn_array(arr) < 0) return -1;
    }

    memcpy(arr->array + arr->len, data, data_size);
    arr->len += data_size;

    return 0;
}
