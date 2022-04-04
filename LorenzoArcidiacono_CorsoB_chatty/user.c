#include <user.h>
#include <configuration.h> 
#include <message.h>
#include <ops.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @author Lorenzo Arcidiacono, 534235, arci0066@gmail.com
 * Si dichiara che il programma è in ogni sua parte opera originale dell'autore.
 *
 */

///---------- macro per il controllo degli errori ---------
///macro per le funzioni ritornano NULL in caso di errore
#define CHECK_NULL(a,b,r) \
if((a = b)==NULL){perror(r);exit(EXIT_FAILURE);}
///macro per le chiamate che ritornano -1 in caso di errore#include <unistd.h>
#define CHECK_NEG(a,b,r) \
if((a = b)==-1){perror(r);exit(EXIT_FAILURE);}
///---------------------------------------------------

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
int setUsr(char* name,int fd, user* usr, int dim){

	if(name == NULL || fd < 0 || usr == NULL || dim < 0)
		return -1;	
	
    strcpy(usr->nick,name);
    usr->fd_u = fd;
    usr->st = OFFLINE;
    usr->hst.max = dim;
    CHECK_NULL(usr->hst.record,(message_t**) malloc(dim*sizeof(message_t*)),"Allocating Memory");
    memset(usr->hst.record, 0, dim*sizeof(message_t*));
    for(int i = 0; i < dim; i++){
    	usr->hst.record[i]= NULL;
	}
    usr->hst.length = 0;
    return 0;
}


/**
 * @function connUsr
 * @brief Connette un user dell'array.
 *
 * @param usr utente da connettere;
 * @param fd file descriptor dell'utente
 * @return  0 in caso di successo, -1 altrimenti.
 */
int connUsr(user* usr,int fd){
    usr->st = ONLINE;
    usr->fd_u = fd;
    return 0;
}

/**
 * @function discUsr
 * @brief Disconnette un user dell'array.
 *
 * @param usr Utente da disconnettere;
 * @return  0 in caso di successo, -1 altrimenti.
 */
int discUsr(user* usr){
    usr->st = OFFLINE;
    return 0;
}


/**
 * @function addMsg
 * @brief Aggiunge un messaggio alla history.
 *
 * @param usr Utente che deve ricevere il messaggio.
 * @param msg Messaggio da inserire.
 * @return 0 in caso di successo, -1 altrimenti;
 */
int addMsg(user* usr, message_t *msg){
    
    if(usr == NULL || msg == NULL)
        return -1;
    
    int ind = (usr->hst.length)%(usr->hst.max); //i messaggi non vengono cancellati ma riscritti
    
    if(usr->hst.record[ind] != NULL){	//se era già stato usato cancello la memoria
    	free(usr->hst.record[ind]->data.buf);
    	free(usr->hst.record[ind]);
    }
    
    message_t* to_add;
    CHECK_NULL(to_add, (message_t*) malloc(sizeof(message_t)), "allocating history message");
    memset(to_add,0,sizeof(message_t));
    
    //copio l'header del messaggio
    strcpy(to_add->hdr.sender, msg->hdr.sender);
    to_add->hdr.op = msg->hdr.op;
    
    //copio la parte dati del messaggio
    strcpy(to_add->data.hdr.receiver,msg->data.hdr.receiver);
    CHECK_NULL(to_add->data.buf, (char*) malloc(msg->data.hdr.len*sizeof(char)),"allocating history message buf");
    memset(to_add->data.buf, 0, msg->data.hdr.len*sizeof(char));
    strcpy(to_add->data.buf,msg->data.buf);
    to_add->data.hdr.len = msg->data.hdr.len;    
   
    usr->hst.record[ind] = to_add;    //inserisce il messaggio nella memoria
    usr->hst.length++;      //Incrementa il numero di messaggi pendenti
    return 0;
}


/**
 * @function getHist
 * @brief Restituisce i messaggi pendenti dell'utente.
 *
 * @param usr Utente di cui si vogliono i messaggi.
 * @return ritorna l'array dei messaggi in caso di successo, NULL se l'array è vuoto o usr == NULL.
 */
message_t* getHist(user* usr){
    
    if(usr == NULL){
        return NULL;
    }
    
    int len = getHistSize(usr);
    
    if(len == 0 && usr->hst.record[0]->data.buf == NULL){ //se non ci sono messaggi pendenti
        return NULL;
    }
    
    message_t *tmp;
    CHECK_NULL(tmp, (message_t*) malloc(len*sizeof(message_t)),"allocating history copy");
    memset(tmp, 0, len*sizeof(message_t));
    int max = getHistSize(usr);
    for(int i = 0; i < max;i++){
        tmp[i] = *usr->hst.record[i];
    }
    
    return tmp;
}

/**
 * @function getHist
 * @brief Restituisce il numero di messaggi pendenti dell'utente.
 *
 * @param usr Utente di cui si vuole il numero di messaggi.
 * @return Restituisce il numero di messaggi pendenti.
 */
int getHistSize(user* usr){
   
   if(usr == NULL)
   		return -1; 
   
    if(usr->hst.length == 0)	//caso non abbia ricevuto messaggi
        return 0;
    if((usr->hst.length)>(usr->hst.max))	//caso abbia sovrascritto alcuni vecchi
        return  (usr->hst.length)%(usr->hst.max);
    return usr->hst.length;
}

/**
 * @function getFd
 * @brief Ritorna il descrittore di un user.
 *
 * @param usr Utente di cui si vuole il fd ;
 * @return  fd_u in caso di successo, -1 altrimenti.
 */
int getFd(user* usr){
	if(usr == NULL) 
		return -1;
    return usr->fd_u;
}


/**
 * @function getStatus
 * @brief Ritorna lo stato di un user.
 *
 * @param usr Utente;
 * @return  Lo status in caso di successo, -1 altrimenti.
 */
int getStatus(user* usr){
	if(usr == NULL) 
		return -1;
    return usr->st;
}


/**
 * @function getNick
 * @brief Ritorna il nickname di un user.
 *
 * @param user* Utente;
 * @return  nick in caso di successo, NULL altrimenti.
 */
char* getNick(user* usr){
	if(usr == NULL) 
		return NULL;
    return usr->nick;
}

/**
 * @function freeUsr
 * @brief libera la memoria dell'utente.
 *
 * @param usr Utente.
 */
void freeUsr(user* usr){
	if(usr == NULL) 
		return ;
	for(int i = 0; i < usr->hst.max;i++){
		if(usr->hst.record[i] != NULL){
        	free(usr->hst.record[i]->data.buf);
        	free(usr->hst.record[i]);
        }
    }
	free(usr->hst.record);
	return ;
}
	















