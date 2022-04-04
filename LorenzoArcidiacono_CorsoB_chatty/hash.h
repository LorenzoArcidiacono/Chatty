#ifndef HASH_H
#define HASH_H

#include <pthread.h>
#include <user.h>

/**
 * @author Lorenzo Arcidiacono, 534235, arci0066@gmail.com
 * Si dichiara che il programma è in ogni sua parte opera originale dell'autore.
 *
 */

/**
 * @file libhash.h
 * @brief Libreria per la gestione di una tabella hash di utenti con chiavi intere.
 * @note Lieve modifica del lavoro di Jakub Kurzak.
 */


/**
 * @struct hash_entry
 * @brief Struttura delle entry della tabella hash.
 *
 * @var key Chiave legata alla entry (unica e non ripetibile).
 * @var usr User relativo alla chiave.
 * @var next entry successiva dello stesso bucket.
 */

typedef struct hash_entry_s{
    char* key;
    user *usr;
    struct hash_entry_s* next;
} hash_entry;


/**
 * @struct hash_table
 * @brief Struttura della tabella hash.
 *
 * @var nbuckets Numero di bucket totali.
 * @var nentries Numero di elementi inseriti.
 * @var connected Numero di utenti connessi.
 * @var bucket Spazi in cui inserire gli elementi.
 * @var hash_fun Funzione che calcola il bucket in cui inserire l'elemento.
 * @var hash_key_compare Funzione che compara due chiavi.
 */


typedef struct hash_table_s{
    pthread_mutex_t *mtx;
    int nbuckets; //num di spazi liberi
    int nentries; //num elem già inseriti
    int connected; //num utenti connessi
    hash_entry **buckets; //spazi in cui inserire le entry
    unsigned int (*hash_fun)(char *key); //funzione hash
    int (*hash_key_compare)(char*, char*); //funzione di comparazione tra chiavi
} hash_table;


/**
 * @function hash_create
 * @brief Funzione che crea la tabella e alloca lo spazio di questa e dei relativi bucket.
 * @param buck Numero di bucket.
 * @param hash_fun hash_fun description
 * @param hash_key_compare Funzione per comaprare le chiavi.
 * @return Ritorna la tabella hash in caso di successo, NULL altrimenti.
 */
hash_table* hash_create(int buck,unsigned int (*hash_fun)(char *key),int (*hash_key_compare)(char*, char*));


/**
 * @function hash_insert
 * @brief Inserisce un elemento nella tabella hash.
 *
 * @param hash Tabella hash.
 * @param key Chiave dell'elemento.
 * @param data User da inserire
 * @return 0 in caso di successo, -1 altrimenti.
 */
int hash_insert(hash_table* hash, char *key, char* name, int fd, int msg_dim);


/**
 * @function hash_delete
 * @brief Elimina un elemento dalla tabella hash e libera la memoria di questo.
 *
 * @param hash Tabella hash.
 * @param key Chiave dell'elemento.
 * @return 0 in caso di successo, 1 in caso l'elemento non esista, -1 in caso di errore;
 */
int hash_delete(hash_table* hash, char* key);


/**
 * @function hash_find
 * @brief Cerca un elemento nella tabella hash.
 *
 * @param hash Tabella hash.
 * @param key Chiave da cercare.
 * @return L'elemento in caso di successo, NULL altrimenti.
 */
hash_entry* hash_find(hash_table* hash, char* key);


/**
 * @function hash_getInfo
 * @brief Cerca l'utente nella tabella e setta i parametri != NULL passati alla funzione con i
 *        valori dell'utente.
 *
 * @param ht Tabella hash degli utenti.
 * @param key Chiave dell'utente.
 * @param fd File descriptor dell'utente.
 * @param st Status dell'utente.
 * @return Setta i parametri fd e st, se != NULL, e ritorna 0 in caso di successo,1 se non trova l'utente, -1 altrimenti.
 */
int hash_getInfo(hash_table* ht,char* key,int *fd, int *st);


/**
 * @function hash_getAllFd
 * @brief Inserisce nell'array passato tutti i file descriptor degli utenti.
 *
 * @param ht Hash table degli utenti
 * @param arr array in cui inserire i file descriptor
 * @return ritorna il numero di file descriptor in caso di successo, -1 altrimenti
 */
int hash_getAllFd(hash_table* ht, int* arr);


/**
 * @function hash_getUSer
 * @brief ritorna la lista degli utenti registrati
 *
 * @param ht Hash Table degli utenti.
 * @param dim Variabile in cui scrivere il numero di utenti registrati.
 * @return Gli utenti registrati in caso di successo, NULL altrimenti.
 */
char* hash_getUsers(hash_table* ht,int *dim);


/**
 * @function hash_connect
 * @brief Segna un cliente come connesso.
 
 * @param ht Hash table dei client.
 * @param key Chiave del client da connettere.
 * @return ritorna 0 in caso di successo, -1 altrimenti;
 */
int hash_connect(hash_table *ht,char* key,int fd);


/**
 * @function hash_disconnect
 * @brief Segna un cliente come disconnesso.
 
 * @param ht Hash table dei client.
 * @param key Chiave del client da connettere.
 * @return ritorna 0 in caso di successo, -1 altrimenti;
 */
int hash_disconnect(hash_table *ht,char* key);


/**
 * @function hash_saveMsg
 * @brief Aggiunge un messaggio alla history di un utente.
 *
 * @param ht Hash table dei client.
 * @param key Chiave del client.
 * @param msg Messaggio da aggiungere.
 * @return ritorna OP_OK in caso di successo, OP_FULL_HIST se la history è piena, OP_FAIL altrimenti;
 */
int hash_saveMsg(hash_table* ht,char *key, message_t* msg);


/**
 * @function hash_getHist
 * @brief Ritorna tutti i messaggi presenti nella history dell'utente.
 *
 * @param ht Hash table degli utenti.
 * @param key Chiave dell'utente.
 * @return La history dell'utente in caso di successo,NULL se la lista è vuota o in caso di errore.
 */
message_t* hash_getHist(hash_table* ht, char* key, int* dim);


/**
 * @function hash_getAllKey
 * @brief restituisce tutte le chiavi degli utenti e le scrive nel vettore passato come argomento
 *
 * @param ht Hash Table degli utenti.
 * @param vec vettore dove scrivere le chiavi
 * @return il numero di chiavi salvate
 */
int hash_getAllKey(hash_table* ht, char** vec);


/**
 * @function hash_fdDisconnect
 * @brief disconnette l'utente corrispondete al file descriptor passato come argomento
 *
 * @param ht Hash Table degli utenti.
 * @param fd File Descriptor dell'utente da eliminare
 * @return 0 in caso di successo, -1 altrimenti
 */
int hash_fdDisconnect(hash_table* ht, int fd);


/**
 * @function hash_getDim
 * @brief   Ritorna il numero di utenti nella tabella.
 *
 * @param ht Hash table degli utenti.
 * @return Ritorna il numero di utenti nella tabella.
 */
int hash_getDim(hash_table* ht);


/**
 * @function hash_getConn
 * @brief   Ritorna il numero di utenti connessi.
 *
 * @param ht Hash table degli utenti.
 * @return Ritorna il numero di utenti connessi.
 */
int hash_getConn(hash_table* ht);


/**
 * @function hash_destroy
 * @brief Cancella la tabella hash e libera la memoria.
 *
 * @param hash Tabella hash.
 * @return 0 in caso di successo, -1 altrimenti.
 */
int hash_destroy(hash_table* hash);


/**
 * @function hash_print
 * @brief Stampa tutti gli elementi della tabella hash e relativi attributi.
 *
 * @param hash Tabella hash.
 * @param out File descriptor del file in cui stampare.
 */
void hash_print(hash_table* hash, FILE* out);

#endif
