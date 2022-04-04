/*
 * membox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 *
 */

/**
 * @author Lorenzo Arcidiacono, 534235, arci0066@gmail.com
 * Si dichiara che il programma è in ogni sua parte opera originale dell'autore.
 *
 */
 
/**
 * @file chatty.c
 * @brief File principale del server chatterbox
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <configuration.h>
#include <connections.h>
#include <message.h>
#include <ops.h>
#include <stats.h>
#include <config.h>
#include <lista.h>
#include <hash.h>

//---------- macro per il controllo degli errori ---------
///macro per le funzioni ritornano NULL in caso di errore
#define CHECK_NULL(a,b,r){\
if((a = b)==NULL){perror(r);exit(EXIT_FAILURE);}}
///macro per le chiamate che ritornano -1 in caso di errore#include <unistd.h>
#define CHECK_NEG(a,b,r){\
if((a = b)==-1){perror(r);exit(EXIT_FAILURE);}}

//---------------------------------------------------

//-------------- Funzioni di utilità per il server --------------
///thread master che gestisce i client
void* master(void* args);
///thread worker che gestisce le operazioni di un singolo client
void* worker(void* args);
///funzione che libera la memoria al momento della chiusura del server
int clearAll();
///istruzioni per l'avvio
static void usage(const char *progname);
///funzione per la chiusura del server all'arrivo di un segnale
static void quit(int signum);
///funzione per l'invio delle statistiche all'arrivo di un segnale
static void stats(int signum);
///funzione per l'aggiornamento delle statistiche
void updateStat(int nuser,int online,int sent, int waiting,int fsent, int fwaiting,int error);
///funzione per la stampa delle statistiche
void print_Stat();
//----------------------------------------------------

//---------------- Operazioni del server ----------------------
///Connette un utente e gli invia la lista degli utenti connessi.
int connectOp(message_hdr_t *hdr,int fd,hash_table *ht);
///Registra e connette un utente e gli invia la lista degli utenti connessi.
int registerOp(message_hdr_t *hdr, int fd,hash_table *ht);
///Invia un messaggio a un altro utente.
int postTxtOp(message_t *msg, int fd, hash_table *ht);
///Invia un messaggio a tutti gli utenti.
int postTxtAllOp(message_t *msg, int fd, hash_table *ht);
///Invia un file a un altro utente.
int postFileOp(message_t *msg, int fd, hash_table *ht);
///Scarica un file ricevuto.
int getFileOp(message_t *msg, int fd, hash_table *ht);
///Riceve tutti i messaggi nella history.
int getPrevMsgOp(message_hdr_t *hdr, int fd, hash_table *ht);
///Riceve la lista degli utenti connessi.
int UsrListOp(message_hdr_t *hdr, int fd, hash_table *ht);
///Cancella l'utente da quelli registrati.
int unregisterOp(message_hdr_t *hdr,int fd, hash_table *ht);
///Disconnette l'utente.
int disconnectOp(message_hdr_t *hdr, int fd, hash_table *ht);

//-----------------------------------------------------------

//---- Variabili Globali per la Gestione dei Thread ----
pthread_t *tid; ///array dei thread_id dei worker
pthread_t tidm; ///thread_id del master thread
volatile sig_atomic_t end_t = 1; ///Variabile per l'interruzione dei thread.
//----------------------------------------------------

//------------- Variabile di Mutua Esclusione per il set -----------
static pthread_mutex_t mtx_set = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t set_empty = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t mtx_stat = PTHREAD_MUTEX_INITIALIZER;
//------------------------------------------------------------------

//--------- Variabili Globali per il Socket e per il set -----------
lista list;     ///Lista dei fd relativi ai client che hanno richieste.
int fd_sock;    ///fd del socket di connessione.
fd_set set;  ///set di fd per da usare nella select: fd attivi.
struct sockaddr_un sock;  ///Indirizzo del socket.
int fd_max = 0; ///massimo indice di descrittore attivo.
static struct conf configuration = { 0, 0, 0, 0, 0, NULL, NULL, NULL};
//----------------------------------------------------

//---------- Variabili Globali per il Server -------------
struct statistics chattyStats = { 0,0,0,0,0,0,0 }; /// struttura che memorizza le statistiche del server, struct statistics e' definita in stats.h.
FILE* stat_out; ///File dove scrivere le statistiche all'arrivo del segnale.
volatile sig_atomic_t sigusr = 0; ///Variabile per la richiesta di stampa delle statistiche.
hash_table* ht;  ///Memoria degli utenti registrati. Definita in libhash.h .
//----------------------------------------------------


//------------------ MAIN -----------------------------
int main(int argc, char* argv[]){
    if(argc < 2){
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }
    printf("AVVIO SERVER...\n");
    FILE* setting; //Descrittore del file di configurazione
    int err = 0; //Variabile per il controllo degli errori.
    sigset_t ss; //set dei segnali da usare per la maschera
    struct sigaction sa; //Variabile per il socket.
    
    ///------------- Inizializzazione Server -----------------
    
    configuration.conn=0;
	configuration.pool=0;
	configuration.msg_dim=0;
	configuration.file=0;
	configuration.hist=0;

	configuration.path=NULL;
	configuration.dir=NULL;
	configuration.stat_file=NULL;
    
    //inizializzo il server in base al file di configurazione
    CHECK_NULL(setting,fopen(argv[2],"r"),"Opening File");
    CHECK_NEG(err,init(setting,&configuration),"Setting Server\n");
    fclose(setting);
    
    //creo e inizializzo il socket
    errno = 0;
    err = unlink(configuration.path);  //Elimino un eventuale link precendente
    if(err == -1 && errno != ENOENT){   //Se il path esiste e c'è un errore
        exit(errno);
    }
    
    memset(&sock, 0, sizeof(struct sockaddr_un));
    sock.sun_family = AF_UNIX;
    strncpy(sock.sun_path, configuration.path, UNIX_PATH_MAX);
    CHECK_NEG(fd_sock,socket(AF_UNIX,SOCK_STREAM,0),"Socket create");
    
    CHECK_NEG(err,bind(fd_sock,(const struct sockaddr *)&sock, sizeof(sock)),"bind");
    
    
    //imposto il segnale di chiusura del server e gli altri signal_handler
    sigfillset(&ss);
    pthread_sigmask(SIG_SETMASK ,&ss, NULL); //maschero tutti i segnali per non essere interrotto
    memset(&sa,0,sizeof(sa));
    
    //segnali di chiusura
    sa.sa_handler = quit;
    sigaction(SIGINT,&sa,NULL);
    sigaction(SIGQUIT,&sa,NULL);
    sigaction(SIGTERM,&sa,NULL);

    //imposto il sengale per non essere chiuso da una SIGPIPE
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE,&sa,NULL);
    
    //imposto il segnale per l'invio delle statistiche
    sa.sa_handler = stats;
    sigaction(SIGUSR1,&sa,NULL);
    
    sigemptyset(&ss);
    pthread_sigmask(SIG_SETMASK ,&ss, NULL); //riattivo tutti i segnali
    
    //Inizializzo l'hash_table e la lista
    err = initialize(&list,configuration.conn);
    if(err != 0)
        exit(err);
    ht = hash_create(configuration.conn,NULL,NULL);
    if(ht == NULL)
        exit(EXIT_FAILURE);
    
    ///-------------------------------------------------------
    
    //creo il thread master e il pool.
    if((err = pthread_create(&tidm,NULL,&master,NULL))!=0){
        printf("ERRORE CREANDO IL TRHEAD MASTER\n");
        exit(EXIT_FAILURE);
    }
    
    tid = (pthread_t*) malloc(configuration.pool*sizeof(pthread_t));
    memset(tid,0,configuration.pool*sizeof(pthread_t));
    for(int i = 0; i < configuration.pool; i++){
        if((err = pthread_create(&tid[i],NULL,&worker,NULL))!=0){
            printf("ERRORE CREANDO IL THREAD WORKER[%d]\n",i);
            exit(EXIT_FAILURE);
        }
    }
    
    printf("SERVER PRONTO\n");
    
    //Aspetto che terminino i thread.
    pthread_join(tidm,NULL);
    printf("THREAD MASTER TERMINATO CORRETTAMENTE\n");
    
    for( int i = 0; i < configuration.pool; i++){
        pthread_join(tid[i],NULL);
        printf("THREAD WORKER[%d] TERMINATO CORRETTAMENTE\n",i);
    }
    
    //Libero la memoria.
    clearAll();
    for (size_t i = 0; i <= fd_max; i++) {
        //chiudo i fd che potrebbero essere rimasti aperti
        if (shutdown(i, SHUT_RDWR) == -1) {
            if (errno != ENOTSOCK) {
                perror("shutdown in master");
            }
        }
    }
    printf("SERVER TERMINATO CORRETTAMENTE\n");
    fflush(stdout);
    return 0;
}

//------------------------------------------------------------------------------


//------------------ Routine dei thread ----------------------
/**
 * @function master
 * @brief Funzione che gestisce il master thread.
 *
 * @param args Argomenti passati al master thread.
 * @return 0 in caso di successo -1 altrimenti
 */
void* master(void* args){
    int ciclo = -1;
    int ok = 0, fd_c, err; //fd_c: variabile per salvare temporaneamente il descrittore di un client.
    fd_set rset; //copia modificabile del set.
    
    //Si mette in ascolto sul socket
    CHECK_NEG(ok,listen(fd_sock,configuration.conn),"Listen");
    if(fd_sock > fd_max)
        fd_max = fd_sock;
    
    //Inizializza il set
    pthread_mutex_lock(&mtx_set);
    FD_ZERO(&set);
    FD_SET(fd_sock,&set);
    pthread_mutex_unlock(&mtx_set);
    
    // imposta il timeout per la select
    struct timeval tv = {0, 0};

    while(end_t){
    	if(sigusr){     //alla ricezione di SIGUSR1 stampa le statistiche
    		print_Stat();
    		sigusr = 0;
    	}
        ciclo++;
        pthread_mutex_lock(&mtx_set);
        rset = set;
        pthread_mutex_unlock(&mtx_set);
        int dim = fd_max;
        
        //Controlla quali utenti hanno scritto
        errno = 0;
        err = select(fd_max+1,&rset,NULL,NULL,&tv);
        if(err == -1){
            perror("Select");
            exit(errno);
        }
        if(err == 0)
            continue;
        else{
            for (int fd = 0; fd <= dim; fd++) {
                if(!FD_ISSET(fd,&rset))
                    continue;
                else{
                    if(fd == fd_sock){  //richesta di connessione di un client
                        fd_c = accept(fd_sock,NULL,0);
                        pthread_mutex_lock(&mtx_set);
                        FD_SET(fd_c,&set);
                        pthread_mutex_unlock(&mtx_set);
                        if(fd_c > fd_max)
                            fd_max = fd_c;
                    }
                    else{       //richiesta di un client di scambiare un messaggio
                        //Tolglie il File Descriptor dal set e lo inserisce nella lista
                        pthread_mutex_lock(&mtx_set);
                        FD_CLR(fd,&set);
                        pthread_mutex_unlock(&mtx_set);
                 
                        CHECK_NEG(ok,set_list(fd,&list),"set list");
                        pthread_cond_signal(&list.empty);
                    }
                }
            }
        }
    }
    end_list(&list); //Setta la lista come da chiudere, facendo uscire i thread Worker dall'attesa in lettura o scrittura
    pthread_cond_broadcast(&list.empty);
    pthread_cond_broadcast(&list.full);
    
    pthread_exit(NULL);
}
    
    
/**
 * @function worker
 * @brief Funzione che determina il comportamento di un thread del pool.
 *
 * @param args Argomenti passati al worker thread.
 * @return 0 in caso di successo -1 altrimenti*
 */
void* worker(void* args){
    message_t msg; //Per copiare il messaggio letto.
    int fd_t, ok; //fd_t: descrittore della connessione su cui lavora il thread. ok: variabile per il controllo degli errori.
    //char* sender;
    sleep(1);
    while(end_t){
        msg.hdr.op = -1;
        strcpy(msg.hdr.sender ,"");
        CHECK_NEG(fd_t,get(&list),"Getting fd");  //Riceve un fd e lo toglie dalla lista, se è vuota aspetta
        //if(!end_t) break; //dopo la get controllo se devo terminare il thread
        ok = readMsg(fd_t,&msg);
        if(ok <= 0 ){  //Socket utente chiuso
            hash_fdDisconnect(ht,fd_t);
           	updateStat(0,-1,0,0,0,0,0);
            pthread_mutex_lock(&mtx_set);
            FD_CLR(fd_t,&set);
            pthread_mutex_unlock(&mtx_set);
            continue;
        }
        else{   //seleziona l'operazione da fare
            switch (msg.hdr.op) {
                case CONNECT_OP:
                    ok = connectOp(&msg.hdr, fd_t, ht);
                    break;
                case REGISTER_OP:
                    ok = registerOp(&msg.hdr, fd_t,ht);
                    break;
                case POSTTXT_OP:
                    ok = postTxtOp(&msg,fd_t,ht);
                    break;
                case POSTTXTALL_OP:
                    ok = postTxtAllOp(&msg,fd_t,ht);
                    break;
                case POSTFILE_OP:
                    ok = postFileOp(&msg,fd_t,ht);
                    break;
                case GETFILE_OP:
                    ok = getFileOp(&msg,fd_t,ht);
                    break;
                case GETPREVMSGS_OP:
                    ok = getPrevMsgOp(&msg.hdr,fd_t,ht);
                    break;
                case USRLIST_OP:
                    ok = UsrListOp(&msg.hdr,fd_t,ht);
                    break;
                case UNREGISTER_OP:
                    ok = unregisterOp(&msg.hdr,fd_t,ht);
                    break;
                case DISCONNECT_OP:
                    ok = disconnectOp(&msg.hdr,fd_t,ht);
                    break;
                    
                default:
                    printf("ERRORE: OPERAZIONE NON DEFINITA\n");
                    setHeader(&msg.hdr,OP_FAIL,"");
                    CHECK_NEG(ok,sendHeader(fd_t,&msg.hdr), "Sending header worker");
                    updateStat(0,0,0,0,0,0,-1);
                    ok = -1;
                    break;
            }
            
            if(ok >= 0){    //Se il descrittore non è stato chiuso e l'operazione ha terminato senza errori
                            //Rimetto il File Descriptor nel fd_set
                printf("OPERAZIONE %d TERMINATA CORRETTAMENTE\n",msg.hdr.op);
                free(msg.data.buf);
                msg.data.buf = NULL;
                
                pthread_mutex_lock(&mtx_set);
                FD_SET(fd_t,&set);
                pthread_mutex_unlock(&mtx_set);
            }
            else{	//caso il client sia chiuso o ci sia stato un errore
                printf("OPERAZIONE %d TERMINATA CON ERRORE\n",msg.hdr.op);
                free(msg.data.buf);
                msg.data.buf = NULL;
                
                pthread_mutex_lock(&mtx_set);
                FD_CLR(fd_t,&set);
                pthread_mutex_unlock(&mtx_set);
                
                hash_fdDisconnect(ht,fd_t);
                updateStat(0,-1,0,0,0,0,0);
            }
        }
    }
    pthread_exit(NULL);
}

//---------------------------------------------------------------------------


//--------------------- Funzioni di utilità -----------------------------

///Funzione che stampa il corretto comando per lanciare il programma.
static void usage(const char *progname) {
    fprintf(stderr, "Il server va lanciato con il seguente comando:\n");
    fprintf(stderr, "  %s -f conffile\n", progname);
}


///Funzione per la chiusura del server all'arrivo di un segnale.
static void quit(int signum){
    end_t = 0;
    
    pthread_cond_broadcast(&list.empty);
    pthread_cond_broadcast(&list.full);
	
}


///Funzione per l'aggiornamento delle statistiche.
void updateStat(int nuser,int online,int sent, int waiting,int fsent, int fwaiting,int error){
    pthread_mutex_lock(&mtx_stat);
    chattyStats.nusers += nuser;
    chattyStats.nonline += online;
    chattyStats.ndelivered += sent;
    chattyStats.nnotdelivered += waiting;
    chattyStats.nfiledelivered += fsent;
    chattyStats.nfilenotdelivered += fwaiting;
    chattyStats.nerrors += error;
    pthread_mutex_unlock(&mtx_stat);
}

///funzione per la stampa delle statistiche
void print_Stat(){
	FILE* stat_out = fopen(configuration.stat_file, "a");
	
	if(!stat_out){
		printf("Errore aprendo il file delle statistiche\n");
		return;
	}
	pthread_mutex_lock(&mtx_stat);
	printStats(stat_out);
	pthread_mutex_unlock(&mtx_stat);
	fclose(stat_out);
	return;
}

///Funzione per l'invio delle statistiche all'arrivo di un segnale.
static void stats(int signum){
    sigusr = 1;
    return;
}


///Funzione che libera la memoria al momento della chiusura del server
int clearAll(){
    int err;    //Variabile per il controllo degli errori.
    
    //Chiudo le mutex e le conditional variables
    err = pthread_mutex_destroy(&mtx_set);
    if(err != 0){
        exit(err);
    }
    err = pthread_mutex_destroy(&mtx_stat);
    if(err != 0){
        exit(err);
    }
    err = pthread_cond_destroy(&set_empty);
    if(err != 0){
        exit(err);
    }

    //chiudo il socket
    CHECK_NEG(err,shutdown(fd_sock,SHUT_RDWR),"Closing Socket");
    errno = 0;
    err = unlink(configuration.path);  //Elimino il socket dalla cartella
    if(err == -1 && errno != ENOENT){   //Se c'è un errore != ENOENT
        exit(errno);
    }

    //libero le strutture dati allocate
    clear(&list);
    free(tid);
    CHECK_NEG(err,hash_destroy(ht),"Closing hash table");
    
    //libero le variabili di configurazione
    clearAllConf(&configuration);
    return 0;
}


///Funzione per liberare la memoria di un array di stringhe
void freeAll(char** arr, int dim){
    for(int i = 0 ; i < dim; i++)
        free(arr[i]);
    free(arr);
}

//----------------------------------------------------------------------------


//--------------------- Operazioni del Server -------------------------

/**
 * @function registerOp
 * @brief Registra e connette un utente e gli invia la lista degli utenti connessi.
 *
 * @param hdr Header del messaggio.
 * @param fd Descrittore della connessione.
 * @param ht Hash table degli utenti.
 * @return  0 in caso di successo, -1 altrimenti
 */
int registerOp(message_hdr_t *hdr, int fd, hash_table *ht){
    int err;
    message_t reply;

    if(hdr == NULL || fd < 0 || ht == NULL){
        updateStat(0,0,0,0,0,0,1);
        return OP_FAIL;
    }
	printf("COMINCIO REGISTER_OP\n");
    //controllo che il numero di utenti connessi sia coerente col file di configurazione
     if(hash_getConn(ht) >= configuration.conn){
     	setHeader(&reply.hdr,OP_TOO_MANY_USERS,"");
    	CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header reg1");
    	updateStat(0,0,0,0,0,0,1);
    	return -1;
    }

    //l'utente non esiste => lo inserisco, lo connetto e invio la lista degli utenti registrati
    if(hash_find(ht,hdr->sender)  == NULL){

        if((err = hash_insert(ht,hdr->sender,hdr->sender,fd,configuration.hist)) == -1){
        	setHeader(&reply.hdr,OP_FAIL,"");
            CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header reg2");
            updateStat(0,0,0,0,0,0,1);
            return -1;
        }

        updateStat(1,0,0,0,0,0,0);
        return connectOp(hdr,fd,ht);
        
    }
    else{
    	setHeader(&reply.hdr,OP_NICK_ALREADY,"");
        CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Writing header");
        updateStat(0,0,0,0,0,0,1);
        return -1;
    }
}


/**
 * @function connectOp
 * @brief Connette un utente e gli invia la lista degli utenti registrati.
 *
 * @param hdr Header del messaggio.
 * @param fd Descrittore della connessione.
 * @param ht Hash table degli utenti.
 * @return  0 in caso di successo, -1 altrimenti.
 */
int connectOp(message_hdr_t *hdr, int fd, hash_table *ht){
    if(hdr == NULL || ht == NULL){
        updateStat(0,0,0,0,0,0,1);
        return OP_FAIL;
    }
    printf("COMINCIO CONNECT_OP\n");
    int err, len;
    message_t reply;
    char* usrs = NULL;
    
    //controllo che il numero di utenti connessi sia coerente col file di configurazione
    if(hash_getConn(ht) >= configuration.conn){
        setHeader(&reply.hdr,OP_TOO_MANY_USERS,"");
    	CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header conn");
    	updateStat(0,0,0,0,0,0,1);
    	return -1;
    }
    
    //L'utente è già stato registrato
    if( hash_find(ht,hdr->sender) != NULL){
        if((err = hash_connect(ht,hdr->sender,fd)) == -1){
        	setHeader(&reply.hdr,OP_FAIL,"");
            CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header conn0");
            updateStat(0,0,0,0,0,0,1);
            return -1;
        }
        setHeader(&reply.hdr,OP_OK,"");
        
        //invio la lista degli utenti registrati
        if((usrs = hash_getUsers(ht,&len)) == NULL){
        	setHeader(&reply.hdr,OP_FAIL,"");
            CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header conn1");
            updateStat(0,0,0,0,0,0,1);
            return -1;
        }
        setData(&reply.data,hdr->sender,usrs,len*(MAX_NAME_LENGTH+1));
        
        CHECK_NEG(err,sendRequest(fd,&reply),"Sending User List");
        
        updateStat(0,1,0,0,0,0,0);
        free(usrs);
        return err;
    }
    
    else{
    	setHeader(&reply.hdr,OP_NICK_UNKNOWN,"");
        CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header conn2");
        updateStat(0,0,0,0,0,0,1);
        return -1;
    }
}


/**
 * @function postTxtOp
 * @brief Invia un messaggio di testo ad un altro utente.
 *
 * @param msg Messaggio da inviare.
 * @param fd Descrittore della connessione.
 * @param ht Hash table degli utenti.
 * @return  0 in caso di successo, -1 altrimenti
 */

int postTxtOp(message_t *msg, int fd, hash_table *ht){
    
    if(msg == NULL || ht == NULL){
        updateStat(0,0,0,0,0,0,1);
        return -1;
    }
    printf("COMINCIO POSTTXT_OP\n");
    int err,fd_r,conn; //err: controllo degli errori, fd_r: file descriptor del ricevente, conn: status dell'utente
    message_t reply, to_send;
    char* receiver = msg->data.hdr.receiver;
    
    //caso in cui il mittente non sia registrato
    if(hash_find(ht,msg->hdr.sender) == NULL){
    	setHeader(&reply.hdr,OP_NICK_UNKNOWN,"");
        CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header postxt1");
        updateStat(0,0,0,0,0,0,1);
        return -1;
    }
    //controllo che il messaggio non superi la dimensione massima
    if(msg->data.hdr.len > configuration.msg_dim){
    	setHeader(&reply.hdr,OP_MSG_TOOLONG,"");
        CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header postxt2");
        updateStat(0,0,0,0,0,0,1);
        return -1;
    }
    //controllo che il rivecente sia registrato
    if((err = hash_getInfo(ht,receiver,&fd_r,&conn)) != 0 ){   //salvo i dati dell'utente
        if(err == 1){
        	setHeader(&reply.hdr,OP_NICK_UNKNOWN,"");
            CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header postxt3");
        }
        else if(err == -1){
        	setHeader(&reply.hdr,OP_FAIL,"");
            CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header postxt4");
        }
        updateStat(0,0,0,0,0,0,1);
        return -1;
    }
    
    //controlli superati, posso inviare il messaggio
    setHeader(&to_send.hdr, TXT_MESSAGE, msg->hdr.sender);
    setData(&to_send.data, " ", msg->data.buf, msg->data.hdr.len);
    
    if(conn == 1){  //utente connesso.
        CHECK_NEG(err, sendRequest(fd_r,&to_send), "Sending txt_message");
        if((err = hash_saveMsg(ht,receiver,&to_send)) == -1){
        	setHeader(&reply.hdr,OP_FAIL,"");
            CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header postxt5");
            updateStat(0,0,0,0,0,0,1);
            return -1;
            }
        setHeader(&reply.hdr,OP_OK,"");
        CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header postxt6");
        updateStat(0,0,1,0,0,0,0);
        return 0;
    }
    
    else{       //utente non connesso.
        if((err = hash_saveMsg(ht,receiver,&to_send)) == -1){
        	setHeader(&reply.hdr,OP_FAIL,"");
            CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header postxt7");
            updateStat(0,0,0,0,0,0,1);
            return -1;
            }
        setHeader(&reply.hdr,OP_OK,"");
        CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header postxt8");
        updateStat(0,0,0,1,0,0,0);
        return 0;
    }
    return -1;
}


/**
 * @function postTxtAllOp
 * @brief Invia un messaggio di testo a tutti gli utenti.
 *
 * @param msg Messaggio da inviare.
 * @param fd Descrittore della connessione.
 * @param ht Hash table degli utenti.
 * @return  0 in caso di successo, -1 altrimenti
 */

int postTxtAllOp(message_t *msg, int fd, hash_table *ht){
	int err2;
	message_t reply;
    if(msg == NULL || ht == NULL){
    	setHeader(&reply.hdr,OP_FAIL,"");
    	CHECK_NEG(err2,sendHeader(fd,&reply.hdr),"Sending header postall1");
        return OP_FAIL;
    }
    printf("COMINCIO POSTTXTALL_OP\n");
    int err,fd_r,conn; //err: controllo degli errori, fd_r: file descriptor del ricevente, conn: status dell'utente
    message_t  to_send;
    char** vec;
    
    //controllo che il sender sia registrato
    if(hash_find(ht,msg->hdr.sender) == NULL){
    	setHeader(&reply.hdr,OP_NICK_UNKNOWN,"");
        CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header postall2");
        return -1;
    }
    
    //controllo che il messaggio non sia troppo lungo 
    if(msg->data.hdr.len > configuration.msg_dim){
    	setHeader(&reply.hdr,OP_MSG_TOOLONG,"");
        CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header postall3");
        return -1;
    }
    
    //Salvo il numero di utenti registrati
    int dim = hash_getDim(ht);
    if(dim <= 0){
    	setHeader(&reply.hdr,OP_FAIL,"");
        CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header postall4");
        return -1;
    }
    
    //Salvo tutte le chiavi degli utenti
    CHECK_NULL(vec, (char**) malloc(dim*(sizeof(char*))),"Allocating memory");
    memset(vec, 0, dim*(sizeof(char*)));
    for(int i = 0; i < dim; i++){
        CHECK_NULL(vec[i], (char*) malloc((MAX_NAME_LENGTH+1)*(sizeof(char))),"Allocating memory");
        memset(vec[i],0,(MAX_NAME_LENGTH+1)*(sizeof(char)));
    }
    if((err = hash_getAllKey(ht,vec)) != dim){
    	setHeader(&reply.hdr,OP_FAIL,"");
        CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header postall5");
        freeAll(vec,dim);
        return -1;
    }
    
    //posso inviare il messaggio
    setHeader(&to_send.hdr, TXT_MESSAGE, msg->hdr.sender);
    setData(&to_send.data, " ", msg->data.buf, msg->data.hdr.len);
    
    for(int i = 0; i< dim; i++){
        if(strcmp(msg->hdr.sender,vec[i]) == 0){	//Non mando il messaggio al mittente
            continue;
        }
        //strcpy(to_send.data.hdr.receiver, vec[i]);
        if((err = hash_getInfo(ht,vec[i],&fd_r,&conn)) != 0 ){   //salvo i dati dell'utente
            if(err == 1){	//errore tra gli utenti salvati
            	setHeader(&reply.hdr,OP_NICK_UNKNOWN," ");
        		CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header postall6");
        	}
        	else if(err == -1){
        		setHeader(&reply.hdr,OP_FAIL," ");
        		CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header postall7");
       		}
        	freeAll(vec,dim);
        	updateStat(0,0,0,0,0,0,1);
            continue;
        }
        if(conn == 1){  //utente connesso.
            err = sendRequest(fd_r,&to_send);
            if(err < 0){
            	updateStat(0,0,0,0,0,0,1);
                freeAll(vec,dim);
            	return -1;
            }
            if((err = hash_saveMsg(ht,vec[i],&to_send)) == -1){
            	setHeader(&reply.hdr,OP_FAIL," ");
                CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header postall8");
                updateStat(0,0,0,0,0,0,1);
                freeAll(vec,dim);
                return -1;
            }
            else{
                updateStat(0,0,1,0,0,0,0);
            }
        }
        
        else{       //utente non connesso.
            if((err = hash_saveMsg(ht,vec[i],&to_send)) == -1){
            	setHeader(&reply.hdr,OP_FAIL," ");
                CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header postall9");
                updateStat(0,0,0,0,0,0,1);
                freeAll(vec,dim);
                return -1;
            }
            else{
                updateStat(0,0,0,1,0,0,0);
            }
        }
    }
    setHeader(&reply.hdr,OP_OK," ");
    CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header postall10");
    freeAll(vec,dim);
    return 0;
}


/**
 * @function postFileOp
 * @brief Invia un file ad un altro utente.
 *
 * @param msg Messaggio da inviare.
 * @param fd Descrittore della connessione.
 * @param ht Hash table degli utenti.
 * @return  0 in caso di successo, -1 altrimenti
 */
int postFileOp(message_t *msg, int fd, hash_table *ht){
    
    if(msg == NULL || fd < 0 || ht == NULL)
        return -1;
    printf("COMINCIO POSTFILE_OP\n");
    message_data_t file_hdr = msg->data; //salvo il path del file
    message_data_t file_data; //Spazio per il contenuto del file
    message_t reply,to_send;
    char *filename, *receiver;
    FILE* out;
    int err, len_path, fd_r, conn;
    
    readData(fd,&file_data); //ricevo il contenuto del file
    
    //Se file troppo grande
    if(file_data.hdr.len > configuration.file*1024){
        updateStat(0,0,0,0,0,0,1);
        free(file_data.buf);
        setHeader(&reply.hdr,OP_MSG_TOOLONG,"");
        CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header file1");
        return err;
    }
    
    
    //Scrivo il path dove salvare il file
    len_path = strlen(configuration.dir) + file_hdr.hdr.len +2; //1 byte per '/' e uno per '\0'
    filename = (char*) malloc(len_path*sizeof(char));
    memset(filename,0,len_path*sizeof(char));
    
    strncpy(filename,configuration.dir,strlen(configuration.dir));
    strcat(filename,"/");
    
    char* fname = strrchr(file_hdr.buf,'/');    //controllo se nel nome del file è salvato anche il percorso
    if(fname == NULL){  //se non c'è copio direttamente tutto
        strncat(filename,file_hdr.buf,file_hdr.hdr.len);
    }
    else{   //prendo il nome del file senza il percorso
        fname++;
        int length = file_hdr.hdr.len - (fname - file_hdr.buf);
        strncat(filename,fname,length);
    }
    
    //Creo il file nella cartella
    out = fopen(filename,"w");
    if(out == NULL){
        updateStat(0,0,0,0,0,0,1);
        free(filename);
        free(file_data.buf);
        setHeader(&reply.hdr,OP_FAIL,"");
        CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header file2");
        perror("Creating File");
        return -1;
    }
    
    //provo a scrivere il file nella cartella
    err = fwrite(file_data.buf,sizeof(char),file_data.hdr.len,out);
    if(err != file_data.hdr.len){
        perror("Writing file");
        setHeader(&reply.hdr,OP_FAIL,"");
        CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header file3");
        updateStat(0,0,0,0,0,0,1);
        free(filename);
        free(file_data.buf);
        fclose(out);
        return -1;
    }
    fclose(out);
    
    //invio il messaggio al ricevente
    receiver = file_hdr.hdr.receiver;
    setHeader(&to_send.hdr, FILE_MESSAGE, msg->hdr.sender);
    setData(&to_send.data, receiver, msg->data.buf, msg->data.hdr.len);
    
   	if(( err = hash_getInfo(ht,receiver,&fd_r,&conn)) != 0){
    	if(err == 1){
    		setHeader(&reply.hdr,OP_NICK_UNKNOWN," ");
        	CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header file4");
        }
        else if(err == -1){
        	setHeader(&reply.hdr,OP_FAIL," ");
        	CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header file5");
       	}
        updateStat(0,0,0,0,0,0,1);
        free(file_data.buf);
        free(filename);
        return -1;
    }
    
    if(conn == 1){
        CHECK_NEG(err,sendRequest(fd_r,&to_send),"seniding Msg");
        if((err = hash_saveMsg(ht,receiver,&to_send)) == -1){
        	setHeader(&reply.hdr,OP_FAIL," ");
            CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header file6");
            updateStat(0,0,0,0,0,0,1);
            free(filename);
            free(file_data.buf);
            return -1;
        }
        updateStat(0, 0, 0, 0, 1, 0, 0);
    }
    else{
        if((err = hash_saveMsg(ht,receiver,&to_send)) == -1){
        	setHeader(&reply.hdr,OP_FAIL," ");
            CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header file7");
            updateStat(0,0,0,0,0,0,1);
            free(filename);
            free(file_data.buf);
            return -1;
        }
        updateStat(0, 0, 0, 0, 0, 1, 0);
    }
    
    //invio la risposta al mittente
    setHeader(&reply.hdr,OP_OK,"");
    CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header file8");
    
    msg->hdr.op = POSTFILE_OP;
    free(filename);
    free(file_data.buf);
    
    return 0;
}


/**
 * @function getFileOp
 * @brief Scarica un file ricevuto.
 *
 * @param msg Messaggio da inviare.
 * @param fd Descrittore della connessione.
 * @param ht Hash table degli utenti.
 * @return  0 in caso di successo, -1 altrimenti
 */
int getFileOp(message_t *msg, int fd, hash_table *ht){
    
    if(msg == NULL || fd < 0 || ht == NULL)
        return -1;
    printf("COMINCIO GETFILE_OP\n");
    char* path;
    int len_path,file,len_file,err;
    message_t reply;
    struct stat st;
    
    //Scrivo il path dove trovare il file
    len_path = strlen(configuration.dir) + msg->data.hdr.len +2; //1 byte per '/' e uno per '\0'
    path = (char*) malloc(len_path*sizeof(char));
    memset(path,0,len_path);
    
    strcpy(path,configuration.dir);
    strcat(path,"/");
    
    char* filename = strrchr(msg->data.buf,'/'); //controllo se nel nome del file è salvato anche il percorso
    if(filename == NULL){   //se non c'è copio direttamente tutto
        strncat(path,msg->data.buf,msg->data.hdr.len);
    }
    else{       //prendo il nome del file senza il percorso
        filename++;
        int length = msg->data.hdr.len - (filename - msg->data.buf);
        strncat(path,filename,length);
    }
    
    errno = 0;
    file = open(path, O_RDONLY);
    
    //Se il file non esiste o è avvenuto un errore
    if(file == -1){
        if(errno == ENOENT){
        	setHeader(&reply.hdr,OP_NO_SUCH_FILE,"");
            CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header getf1");
        }
        else{
        	setHeader(&reply.hdr,OP_FAIL,"");
            CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header getf2");
        }
        updateStat(0,0,0,0,0,0,1);
        free(path);
        return err;
    }
    
    //Leggo il file
    CHECK_NEG(err,fstat(file,&st),"Reading stat file"); //salvo le statistiche del file
    len_file = st.st_size;  //salvo la dimensione del file

    char* map;
    map = mmap(NULL, len_file, PROT_READ, MAP_PRIVATE, file, 0);    //mappo il file in memoria
    
    // Errore mmap mando errore al client
    if (map == MAP_FAILED) {
        updateStat(0,0,0,0,0,0,1);
        setHeader(&reply.hdr,OP_FAIL,"");
        sendHeader(fd, &reply.hdr);
        updateStat(0,0,0,0,0,0,1);
        free(path);
        close(file);
        return -1;
    }
    // File mappato correttamentes
    else {
        close(file);
        
        //Invio la risposta
        setHeader(&reply.hdr,OP_OK,msg->hdr.sender);
        setData(&reply.data,"",map,len_file);
        CHECK_NEG(err,sendRequest(fd,&reply),"Sending reply");
        
        updateStat(0,0,0,0,1,-1,0);
    }
    munmap(map,len_file);
    close(file);
    free(path);
    return 0;
}



/**
 * @function getPrevMsgOp
 * @brief Riceve tutti i messaggi nella history.
 *
 * @param msg Messaggio da inviare.
 * @param fd Descrittore della connessione.
 * @param ht Hash table degli utenti.
 * @return  0 in caso di successo, -1 altrimenti
 */
int getPrevMsgOp(message_hdr_t *hdr, int fd, hash_table *ht){
    if(hdr == NULL || ht == NULL){
        updateStat(0,0,0,0,0,0,1);
        return OP_FAIL;
    }
    printf("COMINCIO GETPREVIOUS_OP\n");
    int len,err;
    message_t* tmp;
    message_t reply;
    char* nick = hdr->sender;
    
    if(hash_find(ht,hdr->sender) == NULL){  //controllo che il mittente sia registrato
    	setHeader(&reply.hdr,OP_NICK_UNKNOWN,"");
        CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header getprev1");
        updateStat(0,0,0,0,0,0,1);
        return -1;
    }
    //Ricevo la lista dei messaggi pendenti
    tmp = hash_getHist(ht,nick,&len);
    if(len <= 0 || tmp == NULL){   //Risposta in caso non ci siano messaggi pendenti
    	setHeader(&reply.hdr,OP_FAIL,"");
        CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header getprev2");
        free(tmp);
        return -1;
    }
    
    //Risposta al client in caso di messaggi pendenti
    setHeader(&reply.hdr,OP_OK,"");
    setData(&reply.data,"",(char*) &len, sizeof(len));
    CHECK_NEG(err,sendRequest(fd,&reply),"sending reply");
    if(err <= 0){
        updateStat(0,0,0,0,0,0,1);
        free(tmp);
        return -1;
    }
    
    //invio tutti i messaggi
    for(int i = 0; i< len ; i++){
        err = sendRequest(fd,&tmp[i]);
        if(err <= 0){
            updateStat(0,0,0,0,0,0,1);
        }
        else{
            updateStat(0,0,1,-1,0,0,0);
        }
    }
    free(tmp);
    return 0;
}


/**
 * @function unRegisterOp
 * @brief Cancella l'utente da quelli registrati.
 *
 * @param msg Messaggio da inviare.
 * @param fd Descrittore della connessione.
 * @param ht Hash table degli utenti.
 * @return  0 in caso di successo, -1 altrimenti
 */
int unregisterOp(message_hdr_t *hdr,int fd, hash_table *ht){
    
    if(hdr == NULL || ht == NULL){
        updateStat(0,0,0,0,0,0,1);
        return -1;
    }
    printf("COMINCIO UNREGISTER_OP\n");
    int err;
    message_t reply;
    
    if(hash_find(ht,hdr->sender) == NULL){  //controllo che il client sia registrato
    	setHeader(&reply.hdr,OP_NICK_UNKNOWN,"");
        CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header unreg1");
        updateStat(0,0,0,0,0,0,1);
        return -1;
    }

    err = hash_delete(ht,hdr->sender);
    if(err == 0){
        updateStat(-1,0,0,0,0,0,0);
        setHeader(&reply.hdr,OP_OK,"");
        CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header unreg2");
        return 1;
    }
    else if(err == 1){
        updateStat(0,0,0,0,0,0,1);
        setHeader(&reply.hdr,OP_NICK_UNKNOWN,"");
        CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header unreg3");
        return -1;
    }
    else{
    	updateStat(0,0,0,0,0,0,1);
    	setHeader(&reply.hdr,OP_FAIL,"");
        CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header unreg4");
        return -1;
    }
    return -1;
}


/**
 * @function disconnectOp
 * @brief Disconnette l'utente.
 *
 * @param msg Messaggio da inviare.
 * @param fd Descrittore della connessione.
 * @param ht Hash table degli utenti.
 * @return  0 in caso di successo, -1 altrimenti
 */
int disconnectOp(message_hdr_t *hdr,int fd, hash_table *ht){
    if(hdr == NULL || ht == NULL){
        updateStat(0,0,0,0,0,0,1);
        return OP_FAIL;
    }
    printf("COMINCIO DISCONNECT_OP\n");
    int err;
    message_t reply;
    
    if(hash_find(ht,hdr->sender) == NULL){
    	setHeader(&reply.hdr,OP_NICK_UNKNOWN,"");
        CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header disc1");
        updateStat(0,0,0,0,0,0,1);
        return -1;
    }
    
    hash_disconnect(ht,hdr->sender);
    updateStat(0,-1,0,0,0,0,0);
    setHeader(&reply.hdr,OP_OK,"");
    CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header disc2");
    return 0;
}


/**
 * @function UsrListOp
 * @brief Riceve la lista degli utenti registrati.
 *
 * @param msg Messaggio da inviare.
 * @param fd Descrittore della connessione.
 * @param ht Hash table degli utenti.
 * @return  0 in caso di successo, -1 altrimenti.
 */
int UsrListOp(message_hdr_t *hdr, int fd, hash_table *ht){
    
    if(hdr == NULL || ht == NULL || fd < 0){
        updateStat(0,0,0,0,0,0,1);
        return -1;
    }
    printf("COMINCIO USRLIST_OP\n");
    int len,err;
    char* usrs = NULL;
    message_t reply;
    
    if(hash_find(ht,hdr->sender) == NULL){
    	setHeader(&reply.hdr,OP_NICK_UNKNOWN,"");
        CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header list1");
        updateStat(0,0,0,0,0,0,1);
        return -1;
    }
    
    if((usrs = hash_getUsers(ht,&len)) == NULL){    //ricevo la lista degli utenti
    	setHeader(&reply.hdr,OP_FAIL,"");
        CHECK_NEG(err,sendHeader(fd,&reply.hdr),"Sending header list2");
        updateStat(0,0,0,0,0,0,1);
        return -1;
    }
    
    setHeader(&reply.hdr,OP_OK,"");
    setData(&reply.data,hdr->sender,usrs,len*(MAX_NAME_LENGTH + 1));
    CHECK_NEG(err,sendRequest(fd,&reply),"Sending User List");
    
    updateStat(0,0,1,0,0,0,0);
    free(usrs);
    return 0;
}

//--------------------------------------------------------------------



