#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <connections.h>

/**
 * @author Lorenzo Arcidiacono, 534235, arci0066@gmail.com
 * Si dichiara che il programma è in ogni sua parte opera originale dell'autore.
 *
 */

//---------- macro per il controllo degli errori ---------
//macro per le funzioni ritornano NULL in caso di errore
#define CHECK_NULL(a,b,r){\
if((a = b)==NULL){perror(r);exit(EXIT_FAILURE);}}
//macro per le funzioni che settano errno
#define CHECK_ERRNO(a,b,r){\
errno = 0;\
a = b;\
if(errno != 0){perror(r);exit(errno);}}
//macro per le chiamate che ritornano -1 in caso di errore#include <unistd.h>
#define CHECK_NEG(a,b,r){\
if((a = b)==-1){perror(r);exit(EXIT_FAILURE);}}
//macro per il controllo di SIGPIPE
#define CHECK_PIPE(a){\
if( a <= 0 && errno == EPIPE) return 0;\
else if(a < 0) return a;\
else return a;}
//---------------------------------------------------

/**
 * @file  connection.c
 * @brief Implementa le funzioni definite in libconnections.h
 *
 */

/**
 * @function openConnection
 * @brief Apre una connessione AF_UNIX verso il server
 *
 * @param path Path del socket AF_UNIX
 * @param ntimes numero massimo di tentativi di retry
 * @param secs tempo di attesa tra due retry consecutive
 *
 * @return il descrittore associato alla connessione in caso di successo
 *         -1 in caso di errore
 */
int openConnection(char* path, unsigned int ntimes, unsigned int secs){
    struct sockaddr_un sa;
    int fd_skt,err;
    
    //setto i parametri del socket
    strncpy(sa.sun_path, path,UNIX_PATH_MAX);
    sa.sun_family=AF_UNIX;
    CHECK_NEG(fd_skt,socket(AF_UNIX,SOCK_STREAM,0),"Creating Client Socket");
    
    //provo a connettermi
    while((err = connect(fd_skt,(struct sockaddr*)&sa,sizeof(sa))) == -1){
        ntimes--;
        if(ntimes > 0){ //se non sono riuscito a collegarmi riprovo
        	sleep(secs);
        }
        else{
            return -1;
        }
    }
    return fd_skt;
}

// ------------ Server Side ------------
/**
 * @function readHeader
 * @brief Legge l'header del messaggio
 *
 * @param fd     descrittore della connessione
 * @param hdr    puntatore all'header del messaggio da ricevere
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa)
 */
int readHeader(long connfd, message_hdr_t *hdr){
    return recv(connfd, hdr, sizeof(message_hdr_t),0);
}

/**
 * @function readData
 * @brief Legge il body del messaggio
 *
 * @param fd     descrittore della connessione
 * @param data   puntatore al body del messaggio
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa)
 */
int readData(long fd, message_data_t *data){
    int err,l,r;
    char* str;

    //---- Leggo l'header della parte dati ----
    memset(data,0,sizeof(message_data_t));
    errno = 0;
    
    if((err = recv(fd, data,sizeof(message_data_hdr_t),0)) <= 0){
        return err;
    }
    //---- Leggo il messaggio della parte dati ----
    l = data->hdr.len;
    if( l == 0)
        data->buf = NULL;
    else{
    	data->buf = (char*) malloc(l*sizeof(char));
        str = data->buf;
        
        errno = 0;
        
        while( l > 0) {
        	r = recv(fd, str, l, 0);
            
            if( r < 0 ) return r;
            
            str += r;	//mando avanti il puntatore str
            l -= r;		//diminuisco i byte da leggere
        }
    }
    return 1;
}

/**
 * @function readMsg
 * @brief Legge l'intero messaggio
 *
 * @param fd     descrittore della connessione
 * @param data   puntatore al messaggio
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa)
 */
int readMsg(long fd, message_t *msg){
    int err;
    if((err = readHeader(fd,&msg->hdr)) < 0){
        return err;
    }
    //se la connessione è chiusa
    if(err == 0) return 0;
    
    if((err = readData(fd,&msg->data)) < 0){
        printf("readMSg data err %d errno %d\n",err,errno);
        return err;
    }
    //se la connessione è chiusa
    if(err == 0) return 0;
    
    return 1;
}

//-----------------------------------------------

// ---------------- Client Side -----------------

/**
 * @function sendRequest
 * @brief Invia un messaggio di richiesta al server
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return <0 se c'e' stato un errore, 0 in caso il socket sia chiuso , >0 altrimenti
 */
int sendRequest(long fd, message_t *msg){
    int err;
    
    errno = 0;
    if((err = sendHeader(fd,&msg->hdr)) <= 0){
        CHECK_PIPE(err);
    }
    
    errno = 0;
    if((err = sendData(fd,&msg->data)) <= 0){
        CHECK_PIPE(err);
    }
    
    return 1;
}

/**
 * @function sendData
 * @brief Invia il body del messaggio al server
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return <0 se c'e' stato un errore, 0 in caso il socket sia chiuso , >0 altrimenti
 */
int sendData(long fd, message_data_t *msg){
    int err,l,r;
    
    //---- Scrivo l'header della parte dati ----
    errno = 0;
    if((err = send(fd, &msg->hdr, sizeof(message_data_hdr_t), 0)) <= 0){
        CHECK_PIPE(err);
    }
    
    //---- Scrivo il buffer dei dati ----
    errno = 0;
    char* str = msg->buf;
    l = msg->hdr.len;
    while( l > 0) {
        	r = send(fd, str, l, 0);
            
            if( r <= 0 ) return r;
            
            str += r;	//mando avanti il puntatore str
            l -= r;		//diminuisco i byte da scrivere
        }
    
    return 1;
}


/**
 * @function sendData
 * @brief setta e invia l'header del messaggio.
 *
 * @param fd descrittore della connessione.
 * @param msg messaggio da inviare.
 * @return <0 se c'e' stato un errore, 0 in caso il socket sia chiuso , >0 altrimenti
 */
int sendHeader(long fd, message_hdr_t *hdr){
    int err;
    errno = 0;
    err = send(fd, hdr,sizeof(message_hdr_t),0);
    CHECK_PIPE(err);
    return 1;
}






















