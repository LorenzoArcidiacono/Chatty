#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include <lista.h>

/**
 * @author Lorenzo Arcidiacono, 534235, arci0066@gmail.com
 * Si dichiara che il programma è in ogni sua parte opera originale dell'autore.
 *
 */

/**
 * @file liblist.c
 * @brief Implementazione della libreria liblist.h.
 */


/**
 * @function initialize
 * @brief Inizializza tutti i valori della lista e alloca la memoria necessaria.
 *
 * @param list lista da inizializzare.
 * @param max numero massimo di valori da salvare.
 *
 * @return ritorna 0 in caso di successo, >0 in caso di errore di inizializzazione delle mutex dei thread, -1 altrimenti.
 */
int initialize(lista* list,int max){
    int err = 0;
    list->close_list = 0;
    list->ind = -1;
    list->done = -1;
    list->max = max;
    list->value = (int*) malloc(max*sizeof(int));
    if(list->value == NULL){
        perror("Allocating list");
        return -1;
    }
    err = pthread_mutex_init(&list->mtx,NULL);
    if(err != 0)
        return err;
    
    err = pthread_mutex_init(&list->mtx_end,NULL);
    if(err != 0)
        return err;
    
    err = pthread_cond_init(&list->empty,NULL);
    if(err != 0)
        return err;
    
    err = pthread_cond_init(&list->full,NULL);
    if(err != 0)
        return err;
    return 0;
}



/**
 * @function end_list
 * @brief setta la variabile per la chiusura della lista
 *
 * @param list lista di cui settare la variabile
 */
void end_list(lista* list){
    pthread_mutex_lock(&list->mtx_end);
    list->close_list = 1;
    pthread_mutex_unlock(&list->mtx_end);
}


/**
 * @function is_close
 * @brief controlla se una lista è chiusa
 *
 * @param list lista da controllare
 * @return 1 se è chiusa, 0 altrimenti
 */
int is_close(lista* list){
    pthread_mutex_lock(&list->mtx_end);
    int i = list->close_list;
    pthread_mutex_unlock(&list->mtx_end);
    return i;
}

/**
 * @function set_list
 * @brief Aggiunge l'elemento in coda alla lista.
 *
 * @param fd valore da aggiungere.
 * @param list lista da modificare.
 * @return ritorna 0 in caso di successo, -1 altrimenti.
 */
int set_list(int fd,lista* list){
    pthread_mutex_lock(&list->mtx);
    
    while(!is_close(list) && (list->ind)+1 >= list->max ){
        pthread_cond_wait(&list->full,&list->mtx);
    }
    
    if(is_close(list)){ //se è chiusa rilascio la mutex ed esco
        pthread_cond_broadcast(&list->empty);
        pthread_mutex_unlock(&list->mtx);
        return 0;
    }
    
    list->ind++;
    list->value[list->ind] = fd;
    
    pthread_cond_signal(&list->empty);
    pthread_mutex_unlock(&list->mtx);
    return 0;
}


/**
 * @function isEmpty
 * @brief Restituisce 1 se la lista è vuota, 0 altrimenti.
 *
 * @param list lista da controllare.
 * @return Restituisce 1 se ind == -1, 0 altrimenti.
 */
int isEmpty(lista* list){
    int check = 0;
    pthread_mutex_lock(&list->mtx);
    if(list->ind == -1)
        check = 1;
    pthread_mutex_unlock(&list->mtx);
    return check;
}


/**
 * @function get
 * @brief Restituisce un valore della lista con ordine FIFO.
 *
 * @param list lista da cui leggere il valore.
 * @return Restituisce il valore della lista presente da più tempo.
 */
int get(lista* list){
    int fd;
    pthread_mutex_lock(&list->mtx);
    while(!is_close(list) && (list->ind) == -1 ){
        pthread_cond_wait(&list->empty,&list->mtx);
    }

    if(is_close(list)){     // se è chiusa rilascio la mutex ed esco
        pthread_cond_broadcast(&list->empty);
        pthread_mutex_unlock(&list->mtx);
        return 0;
    }
    
    list->done++;
    fd = list->value[list->done];
    if(list->ind == list->done){ //Se sono stati letti tutti, posiziono l'indice a -1;
        list->ind = -1;
        list->done = -1;
    }
   
    pthread_cond_signal(&list->full);
    pthread_mutex_unlock(&list->mtx);
    return fd;
}


/**
 * @function clear
 * @brief Libera la memoria occupatata dalla lista.
 *
 * @param list Lista da liberare.
 */
void clear(lista* list){
    int err;
    free(list->value);
    
    err = pthread_mutex_destroy(&list->mtx);
    if(err != 0){
        exit(err);
    }
    
    err = pthread_mutex_destroy(&list->mtx_end);
    if(err != 0){
        exit(err);
    }
    
    err = pthread_cond_destroy(&list->empty);
    if(err != 0){
        exit(err);
    }
    
    err = pthread_cond_destroy(&list->full);
    if(err != 0){
        exit(err);
    }
    
    return;
}
