#ifndef LIST_H
#define LIST_H

#include <pthread.h>

/**
 * @author Lorenzo Arcidiacono, 534235, arci0066@gmail.com
 * Si dichiara che il programma è in ogni sua parte opera originale dell'autore.
 *
 */

/**
 * @file liblist.h
 * @brief Implementazione di una lista di al più max valori di tipo int, sincronizzata.
 */


/**
 * @struct struct_lista
 * @brief Una lista di massimo max elementi, per la lettura e scrittura sincronizzata.
 *
 * @var ind ultimo elemento inserito.
 * @var close_list variabile per controlloare la chiusura della lista
 * @var max massimo numero di elementi che possono essere inseriti.
 * @var done ultimo elemento di cui è stata fatta la get.
 * @var value lista degli elementi inseriti.
 * @var mtx variabile di mutua esclusione.
 * @var mtx_end variabile di mutua esclusione per lettura/scrittura di close_list
 * @var empty variabile condizionale se la lista è vuota.
 * @var full variabile condizionale se la lista è piena.
 */
typedef struct struct_lista{
    int ind;
    volatile int close_list;
    int max;
    int done;
    int* value;
    pthread_mutex_t mtx;
    pthread_mutex_t mtx_end;
    pthread_cond_t empty;
    pthread_cond_t full;
} lista;


/**
 * @function initialize
 * @brief Inizializza tutti i valori della lista e alloca la memoria necessaria.
 *
 * @param list lista da inizializzare.
 * @param max numero massimo di valori da salvare.
 * @return ritorna 0 in caso di successo, >0 in caso di errore di inizializzazione delle mutex, -1 altrimenti.
 */
int initialize(lista* list, int max);

/**
 * @function end_list
 * @brief setta la variabile per la chiusura della lista
 *
 * @param list lista di cui settare la variabile
 */
void end_list(lista* list);


/**
 * @function is_close
 * @brief controlla se una lista è chiusa
 *
 * @param list lista da controllare
 * @return 1 se è chiusa, 0 altrimenti
 */
int is_close(lista* list);


/**
 * @function set_list
 * @brief Aggiunge l'elemento in coda alla lista.
 *
 * @param fd valore da aggiungere.
 * @param list lista da modificare.
 * @return ritorna 0 in caso di successo, -1 altrimenti.
 */
int set_list(int fd,lista* list);


/**
 * @function isEmpty
 * @brief Restituisce 1 se la lista è vuota, 0 altrimenti.
 *
 * @param list lista da controllare.
 * @return Restituisce 1 se ind == -1, 0 altrimenti.
 */
int isEmpty(lista* list);


/**
 * @function get
 * @brief Restituisce un valore della lista con ordine FIFO.
 *
 * @param list lista da cui leggere il valore.
 * @return Restituisce il valore della lista presente da più tempo.
 */
int get(lista* list);


/**
 * @function clear
 * @brief Libera la memoria occupatata dalla lista.
 *
 * @param list Lista da liberare.
 */
void clear(lista* list);
#endif
