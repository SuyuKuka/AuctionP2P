#ifndef DARRAY_H
#define DARRAY_H

#include <stdlib.h>
#include <pthread.h>

/* Structure pour un tableau de taille variable. */
typedef struct dyn_array {
    void *array; /* Tableau. */
    size_t size; /* Taille totale. */
    size_t len;  /* Taille actuelle. */
} dyn_array;

/* Structure pour un tableau de taille variable. */
typedef struct node {
    void *data;        /* Données. */
    struct node *next; /* Prochain élément. */
    int id;            /* Identifiant */
} node;

/* Structure pour un tableau de taille variable. */
typedef struct linked_list {
    node *head;  /* Premier élément. */
    size_t size; /* Taille. */
    pthread_mutex_t mutex; /* Mutex. */
} linked_list;

/**
 * @brief Initialise `arr` de taille initiale `size`.
 *
 * Si `malloc` échoue, tous les champs sont mis à `0`.
 *
 * @param[in,out]   arr Tableau dynamique.
 * @param[in]       size Taille initiale.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int init_dyn_array(dyn_array *arr, size_t size);
/**
 * @brief Réalloue `arr->array` en doublant sa taille.
 *
 * Si `realloc` échoue, il n'y a aucun changement.
 *
 * @param[in,out]   arr Tableau dynamique.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int realloc_dyn_array(dyn_array *arr);
/**
 * @brief Libère la mémoire utilisée par le tableau dynamique.
 *
 * @param[in]   arr Tableau dynamique.
 */
void free_dyn_array(dyn_array arr);
/**
 * @brief Ajoute `data` à la suite dans le tableau.
 *
 * La méthode réalloue le tableau si la taille des données est plus grande que la
 * taille restante dans le tableau (`realloc_dyn_array`).
 *
 * @param[in,out]   arr Tableau dynamique.
 * @param[in]       data Données.
 * @param[in]       data_size Taille des données.
 *
 * @return `-1` en cas d'erreur, `0` sinon.
 */
int concat_to_array(dyn_array *arr, const void *data, size_t data_size);

/**
 * @brief Alloue un nouveau node.
 *
 * @return `NULL` en cas d'erreur, le nouveau node sinon.
 */
node *init_node();
/**
 * @brief Initialise la liste chaînée.
 * 
 * @param[in,out]   list Liste.
 */
void init_list(linked_list *list);
/**
 * @brief Ajoute le node à la fin de la liste.
 *
 * @param[in,out]   n Tableau dynamique.
 * @param[in]       node_id ID du node.
 * @param[in,out]   list Liste.
 */
void append_to_list(node *n, int node_id, linked_list *list);
/**
 * @brief Retire le node d'ID `id` de la liste.
 *
 * @param[in]       id Identifiant du node.
 * @param[in,out]   list Liste.
 */
void remove_from_list(int id, linked_list *list);
/**
 * @brief Renvoie le dernier node de la liste.
 *
 * @return `NULL` en cas d'erreur ou liste vide, le node sinon.
 */
node *get_last_node(linked_list *list);
/**
 * @brief Renvoie le node avec l'id donné.
 *
 * @return `NULL` si non trouvé, le node sinon.
 */
node *get_node(int id, linked_list *list);
/**
 * @brief Libère la liste chaînée.
 */
void free_list(linked_list *list);

#endif