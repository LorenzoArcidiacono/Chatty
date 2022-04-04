

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <configuration.h>
#include <errno.h>

/**
 * @author Lorenzo Arcidiacono, 534235, arci0066@gmail.com
 * Si dichiara che il programma è in ogni sua parte opera originale dell'autore.
 *
 */

/**
 * @file  libconfiguration.c
 * @brief Contiene le funzioni che settano le variabili di configurazione
 *        del server in base a un file.
 */


//macro per le funzioni ritornano NULL in caso di errore
#define CHECK_NULL(a,b,r){\
if((a = b)==NULL){printf("errore: %s\n",r);exit(EXIT_FAILURE);}}

//macro per le funzioni che settano errno
#define CHECK_ERRNO(a,b,r){\
errno = 0;\
a = b;\
if(errno != 0){printf("errore: %s\n",r);exit(EXIT_FAILURE);}}

//lunghezza massima di parole e frasi
#define SIZEW 25
#define SIZES 5*SIZEW


/**
 * @function get_val
 * @brief Cerca nel file il valore collegato a una serie di stringhe
 *        predefinite.
 *
 * @param fd File Descriptor del file di configurazione.
 * @param str Stringa da cercare.
 * @param dest Valore di ritorno.
 * @return Ritorna la stringa trovata in caso di successo,
 *         NULL altrimenti.
 */
char* get_val(FILE* fd,char* str,char** dest){
    char *sep_f = " ="; //separatori frasi
    char *snt, *tok;
    
    CHECK_NULL(snt,(char*) malloc(SIZES*sizeof(char)),"Allocating snt in get_val\n");
    int len = strlen(str);
 
    rewind(fd);
    
    //leggo tutto il file
    while(fgets(snt,SIZES,fd)!=NULL){
        if(feof(fd) != 0){  //se raggiungo la fine senza trovare il valore
            printf("EOF\n");
            free(snt);
            return NULL;
        }
        if(snt[0] == '#'){   //la frase è un commento
            continue;}
        else{
            tok = strtok(snt,sep_f);
            if(strncmp(str,tok,len)==0){    //la frase è quella cercata
                tok = strtok(NULL,sep_f);
                strcpy(*dest,tok);
                free(snt);
                return *dest;
            }
        }
    }
    free(snt);
    return NULL;
}


/**
 * @function val_n
 * @brief Setta la variabile in base al file fd.
 *
 * @param var Variabile da settare.
 * @param str Stringa da cercare.
 * @param fd File Descriptor del file di configurazione.
 * @return Ritorna il valore trovato.
 */
long int val_n(long int* var,char* str,FILE* fd){
    char* c;
    CHECK_NULL(c,(char*)malloc(SIZEW*sizeof(char)),"allocating c");
    
    //cerco il valore relativo nel file di configurazione
    CHECK_NULL(c, get_val(fd,str,&c),"get_val");
    CHECK_ERRNO(*var,strtol(c,NULL,10),"strtol");
    free(c);
    return *var;
}


/**
 * @function val_s
 * @brief Setta la variabile in base al file fd.
 *
 * @param var Variabile da settare.
 * @param str Stringa da cercare.
 * @param fd File Descriptor del file di configurazione.
 * @return Ritorna il valore trovato.
 */
char* val_s(char* var,char* str,FILE* fd){
    char* c;
    
    CHECK_NULL(c,(char*)malloc(SIZEW*sizeof(char)),"allocating c");
    
    //cerco il valore relativo nel file di configurazione
    CHECK_NULL(c, get_val(fd,str,&c),"get_val");
    if(c[strlen(c)-1] == '\n')  //inserisco il terminatore corretto
        c[strlen(c)-1] = '\0';
    var = (char*) malloc((strlen(c)+1)*sizeof(char));
    strncpy(var,c,strlen(c)+1);
    free(c);
    return var;
}


/**
 * @function init
 * @brief inizializza tutte le variabili di configurazione in base al file
 * @warning Chiamare questa funzione all'avvio del server.
 *
 * @param fd File Descriptor del file di configurazione
 * @param c  Struct dei valori da inizializzare
 *
 * @return 0 in caso di successo
 *         -1 in caso di errore
 */
int init(FILE* fd, struct conf *c ){
    c->conn=0;
	c->pool=0;
	c->msg_dim=0;
	c->file=0;
	c->hist=0;

	c->path=NULL;
	c->dir=NULL;
	c->stat_file=NULL;
    
    //cerco il valore nel file di configurazione
    c->conn = val_n(&c->conn,"MaxConnections",fd);
    c->pool = val_n(&c->pool,"ThreadsInPool",fd);
    c->msg_dim = val_n(&c->msg_dim,"MaxMsgSize",fd);
    c->file = val_n(&c->file,"MaxFileSize",fd);
    c->hist = val_n(&c->hist,"MaxHistMsgs",fd);
    c->path = val_s(c->path,"UnixPath",fd);
    c->dir = val_s(c->dir,"DirName",fd);
    c->stat_file = val_s(c->stat_file,"StatFileName",fd);
    
    //controllo che siano state settate correttamente
    if(c->conn <= 0 || c->pool <= 0 || c->msg_dim <= 0 || c->file <= 0 || c->hist <= 0 || c->path == NULL ||\
    		c->dir == NULL || c->stat_file == NULL ){
    	printf("Errore parsing file\n");
    	return -1;
    }
    
    printf("MaxConn: %ld, NumThread: %ld, MaxMsgSize: %ld, MaxFileSize: %ld, MaxHistSize: %ld, UnixPath: %s, DirName: %s, StatFileName: %s\n",\
    	c->conn,c->pool,c->msg_dim,c->file,c->hist,c->path,c->dir,c->stat_file);

    return 0;
}

/**
 * @function clearAllConf
 * @brief Libera lo spazio in memoria occupato. Da chiamare alla chiusura
 *        del server.
 * @param c Struct dei valori da liberare.
 */
void clearAllConf(struct conf *c ){
    free(c->path);
    free(c->dir);
    free(c->stat_file);
    
    c->path = NULL;
    c->dir = NULL;
    c->stat_file = NULL;
    return ;
}

