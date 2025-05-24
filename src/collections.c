#include "collections.h"

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

node *init_node() {
    node *n = malloc(sizeof(node));
    if (!n) {
        perror("[ERROR] malloc");
        return NULL;
    }

    n->data = NULL;
    n->next = NULL;
    n->id = 0;

    return n;
}

void append_to_list(node *n, int node_id, linked_list *list) {
    if (list->head == NULL) {
        list->head = n;
    } else {
        node *last = list->head;
        while (last->next != NULL) last = last->next;
        last->next = n;
    }
    n->id = node_id;
    list->size++;
}

void remove_from_list(int id, linked_list *list) {
    if (list->head == NULL) return;

    node *tmp = list->head, *prev = NULL;

    if (list->head->id == id) {
        list->head = tmp->next;
        if (tmp->data) free(tmp->data);
        free(tmp);
        list->size--;
        return;
    }

    while (tmp != NULL) {
        if (tmp->id == id) {
            prev->next = tmp->next;
            if (tmp->data) free(tmp->data);
            free(tmp);
            list->size--;
            return;
        }
        prev = tmp;
        tmp = tmp->next;
    }
}

node *get_last_node(linked_list *list) {
    node *tmp = list->head;
    if (!tmp) return NULL;

    while (tmp->next != NULL) tmp = tmp->next;
    return tmp;
}

node *get_node(int id, linked_list *list) {
    for (node *n = list->head; n != NULL; n = n->next) {
        if (n->id == id) return n;
    }

    return NULL;
}

void free_list(linked_list *list) {
    node *curr = list->head;
    node *next;

    while (curr != NULL) {
        next = curr->next;
        free(curr->data);
        free(curr);
        curr = next;
    }

    list->head = NULL;
    list->size = 0;
}
