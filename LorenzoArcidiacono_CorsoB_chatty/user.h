#ifndef USER_H
#define USER_H

#include "configuration.h"
#include "config.h"
#include "message.h"

/**
 * @author Lorenzo Arcidiacono, 534235, arci0066@gmail.com
 * Si dichiara che il programma è in ogni sua parte opera originale dell'autore.
 *
 */

/**
 * @file libuser.h
 * @brief Libreria di funzioni per un utente.
 */

/**
 * @enum status
 * @brief Enumerazione dei possibili stati di un utente.
 */

typedef enum {
    ONLINE = 1,
    OFFLINE  = 0
} status;


/**
 * @struct history
 * @brief Messaggi pendenti dell'utente
 *
 * @param record Array dai messaggi pendenti.
 * @param length Numero di messaggi pendenti.
 * @param max Numero massimo di messaggi salvabili.
 */

typedef struct history_message {
    message_t** record;
    int length;
    int max;
} history;


/**
 * @struct user
 * @brief Struct che contiene le informazioni di un utente registrato.
 *
 * @param nick Nickname dell'utente.
 * @param fd_u File descriptor della connessione con l'utente.
 * @param history Messaggi pendenti dell'utente.
 * @param status Status dell'utente.
 */

typedef struct user_struct{
    char nick[MAX_NAME_LENGTH];
    int fd_u;
    history hst;
    status st;
} user;


/**
 * @function setUsr
 * @brief Setta un user.
 *
 * @param name nickname dell'user.
 * @param fd descrittore della connessione;
 * @param usr utente da connettere;
 * @param dim numero massimo di messaggi nella history
 * @return  0 in caso di successo, -1 altrimenti.
 */
int setUsr(char* name,int fd, user* usr,int dim);


/**
 * @function connUsr
 * @brief Connette un user dell'array.
 *
 * @param usr utente da connettere;
 * @param fd file descriptor dell'utente
 * @return  0 in caso di successo, -1 altrimenti.
 */
int connUsr(user* usr,int fd);


/**
 * @function discUsr
 * @brief Disconnette un user dell'array.
 *
 * @param usr Utente da disconnettere;
 * @return  0 in caso di successo, -1 altrimenti.
 */
int discUsr(user* usr);


/**
 * @function addMsg
 * @brief Aggiunge un messaggio alla history.
 *
 * @param usr Utente che deve ricevere il messaggio.
 * @param msg Messaggio da inserire.
 * @return 0 in caso di successo, -1 altrimenti;
 */
int addMsg(user* usr, message_t *msg);


/**
 * @function getHist
 * @brief Restituisce il numero di messaggi pendenti dell'utente.
 *
 * @param usr Utente di cui si vuole il numero di messaggi.
 * @return Restituisce il numero di messaggi pendenti.
 */

int getHistSize(user* usr);


/**
 * @function getHist
 * @brief Restituisce i messaggi pendenti dell'utente.
 *
 * @param usr Utente di cui si vogliono i messaggi.
 * @return ritorna l'array dei messaggi in caso di successo, NULL se l'array è vuoto o usr == NULL.
 */
message_t* getHist(user* usr);


/**
 * @function getFd
 * @brief Ritorna il descrittore di un user.
 *
 * @param usr Utente di cui si vuole il fd ;
 * @return  fd_u in caso di successo, -1 altrimenti.
 */
int getFd(user* usr);


/**
 * @function getStatus
 * @brief Ritorna lo stato di un user.
 *
 * @param usr Utente;
 * @return  Lo status in caso di successo, -1 altrimenti.
 */
int getStatus(user* usr);


/**
 * @function getNick
 * @brief Ritorna il nickname di un user.
 *
 * @param user* Utente;
 * @return  nick in caso di successo, NULL altrimenti.
 */
char* getNick(user* usr);


/**
 * @function freeUsr
 * @brief libera la memoria dell'utente.
 *
 * @param usr Utente.
 */
void freeUsr(user* usr);

#endif


















