#ifndef DARRAY_H
#define DARRAY_H

#include <stdlib.h>

/* Structure pour un tableau de taille variable. */
typedef struct dyn_array {
    void *array; /* Tableau. */
    size_t size; /* Taille totale. */
    size_t len;  /* Taille actuelle. */
} dyn_array;

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

#endif