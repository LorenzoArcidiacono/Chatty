#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <hash.h>
#include <user.h>
#include <configuration.h>

/**
 * @author Lorenzo Arcidiacono, 534235, arci0066@gmail.com
 * Si dichiara che il programma è in ogni sua parte opera originale dell'autore.
 *
 */

/**
 * @file libhash.c
 * @brief Implementazione della gestione di una tabella hash di utenti con chiavi intere.
 * @note Lieve modifica del lavoro di Jakub Kurzak.
 */

///---------- macro per il controllo degli errori ---------
///macro per le funzioni ritornano NULL in caso di errore
#define CHECK_NULL(a,b,r) \
if((a = b)==NULL){perror(r);exit(EXIT_FAILURE);}

///macro per le chiamate che ritornano -1 in caso di errore#include <unistd.h>
#define CHECK_NEG(a,b,r) \
if((a = b)==-1){perror(r);exit(EXIT_FAILURE);}
///---------------------------------------------------

#define CRITICAL_VALUE 	5 //valore massimo tale che 1 mutex per ogni bucket

///--------- Variabili per la Mutua Esclusione ----------
static int dim;     //numero di blocchi di entrate della tabella hash
static int *activeR; //array che indica il numero di lettori per ogni blocco
static int *activeW; //array che indica il numero di scrittori per ogni blocco
static int *waitingR; //array che indica il numero di lettori in attesa per ogni blocco
static int *waitingW; //array che indica il numero di scrittori in attesa per ogni blocco

static pthread_mutex_t *mtx; //array di variabili di mutua esclusione per la gestione di ogni blocco di bucket
static pthread_cond_t *rGo;  //array delle variabili condizionali su cui aspettano i lettori
static pthread_cond_t *wGo;  //array delle variabili condizionali su cui aspettano gli scrittori

static pthread_mutex_t entry_mtx = PTHREAD_MUTEX_INITIALIZER;      //mtx per la nentries
static pthread_mutex_t conn_mtx = PTHREAD_MUTEX_INITIALIZER;	   //mtx per il numero di utenti connessi
///-------------------------------------------------------


///-------------------------  Funzioni standard per la tabella hash -------------------------


/**
 * @function std_hash_fun
 * @brief Funzione hash della tabella.
 * @param key Chiave dell'elemento.
 * @return Il valore hash.
 */
unsigned int std_hash_fun( char *key ) {
    unsigned char *p = (unsigned char*) key;
    unsigned int h = 2166136261;
    int i = 0,len = 0;
    
    while(key[i] != '\0'){
        len++;
        i++;
    }
    
    for ( i = 0; i < len; i++ )
        h = ( h * 16777619 ) ^ p[i];
    return h;
}


/**
 * @function std_hash_key_compare
 * @brief Funzione per comparare due chiavi intere.
 * @param k1 Chiave di un'elemento.
 * @param k2 Chiave di un'elemento.
 * @return 0 se k1 = k2 != 0 altrimenti.
 */
int std_hash_key_compare(char *k1, char *k2){
    return strcmp(k1,k2);
}


///-----------------------------------------------------------------------------------------------

///-------------------------  Funzioni di Servizio -------------------------

void hash_incDim(hash_table *ht);

void hash_decDim(hash_table *ht);

void hash_conn(hash_table *ht);

void hash_disc(hash_table *ht);

///---------------------------------------------------------------------------------


///------------------------ Funzioni per la Mutua Esclusione --------------------------

int setMtx(hash_table* ht);

void freeMtx();

int initialize_mtx(int dim);

int value(int hash_val);

int startR(int hash_val);

int doneR(int hash_val);

int startW(int hash_val);

int doneW(int hash_val);

int readAll();

int writeAll();

int doneAllR();

int doneAllW();

///-----------------------------------------------------------------------------------------------


/**
 * @function hash_create
 * @brief Funzione che crea la tabella e alloca lo spazio di questa e dei relativi bucket.
 * @param buck Numero di bucket.
 * @param hash_fun hash_fun description
 * @param hash_key_compare Funzione per comaprare le chiavi.
 * @return Ritorna la tabella hash in caso di successo, NULL altrimenti.
 */
hash_table* hash_create(int buck,unsigned int (*hash_fun)(char* key),int (*hash_key_compare)(char*, char*)){
    hash_table* ht;
    
    if(buck <= 0)
        return NULL;
    
    //alloco la tabella
    ht = (hash_table*) malloc(sizeof(hash_table));
    if(ht == NULL) exit(EXIT_FAILURE);
    memset(ht,0,sizeof(hash_table));
    
    //alloco gli spazi della tabella
    ht->nentries = 0;
    ht->buckets = (hash_entry**)malloc(buck * sizeof(hash_entry*));
    if(!ht->buckets){
        free(ht);
        exit(EXIT_FAILURE);
    }
    memset(ht->buckets,0,buck * sizeof(hash_entry*));
    ht->connected = 0;
    
    ht->nbuckets = buck;
    for(int i=0; i<ht->nbuckets; i++){  //inizializzo gli spazi
        ht->buckets[i] = NULL;
    }
    if(hash_fun == NULL)    //Se non viene passata una funzione hash, uso quella standard
        ht->hash_fun = std_hash_fun;
    else
        ht->hash_fun = hash_fun;
    
    if(hash_key_compare == NULL) //Se non viene passata una funzione per comparare le chiavi, uso quella standard
        ht->hash_key_compare = std_hash_key_compare;
    else
        ht->hash_key_compare = hash_key_compare;
    
    //inizializzo le variabili di mutua esclusione e condizionali
    setMtx(ht);
    
    return ht;
}


/**
 * @function hash_insert
 * @brief Inserisce un elemento nella tabella hash.
 *
 * @param hash Tabella hash.
 * @param key Chiave dell'elemento.
 * @param name Stringa contenente il nome dell'utente
 * @param fd File Descriptor dell'utente
 * @param msg_dim numero massimo di messaggi memorizzabili nella history
 * @return 0 in caso di successo, -1 altrimenti.
 */
int hash_insert(hash_table* hash, char* key, char* name, int fd,int msg_dim){
    
    if(hash == NULL || key == NULL || name == NULL || fd < 0 || msg_dim < 0)
        return -1;

    user* new;
    int err = 0;
    hash_entry* curr,*post;
    unsigned int hash_val;

    //alloco e inizializzo il nuovo utente
    CHECK_NULL(new,(user*)malloc(sizeof(user)),"Allocating User");
    memset(new,0,sizeof(user));
    if((err =setUsr(name,fd,new,msg_dim)) == -1){
        free(new);
        return -1;
    }

    hash_val = (* hash->hash_fun)(key) % hash->nbuckets;    //calcolo il valore hash

    startW(hash_val);
    for (curr = hash->buckets[hash_val]; curr != NULL; curr=curr->next){
        if ( (hash->hash_key_compare(curr->key, key)) == 0){ //la chiave è già stata usata
            freeUsr(new);
            free(new);
            doneW(hash_val);
            return -1;
        }
    }
    //se la chiave non è stata già inserita
    post = (hash_entry*)malloc(sizeof(hash_entry));
    post->key = (char*) malloc((strlen(key)+1)*sizeof(char));

    if(post == NULL || post->key == NULL){
        freeUsr(new);
        free(new);
        doneW(hash_val);
        perror("allocating memory");
        exit(EXIT_FAILURE);
    }
    
    strncpy(post->key,key,strlen(key)+1);
    post->usr = new;
    
    //aggiungo l'utente in cima alla lista di trabocco
    if(hash->buckets[hash_val] == NULL)
        post->next = NULL;
    else
        post->next = hash->buckets[hash_val];
    hash->buckets[hash_val] = post;
    doneW(hash_val);
    hash_incDim(hash);  //incremento il numero di utenti registrati
    return 0;
}


/**
 * @function hash_delete
 * @brief Elimina un elemento dalla tabella hash e libera la memoria di questo.
 *
 * @param hash Tabella hash.
 * @param key Chiave dell'elemento.
 * @return 0 in caso di successo, 1 in caso l'elemento non esista, -1 in caso di errore;
 */
int hash_delete(hash_table* hash, char* key){
    
    if(hash == NULL || key == NULL)
        return -1;
    
    hash_entry *curr, *prev;
    unsigned int hash_val = (* hash->hash_fun)(key) % hash->nbuckets;;
    int find = 0;
    
    prev = NULL;
    
    startW(hash_val);
    for (curr=hash->buckets[hash_val]; curr != NULL; )  {
        if ( hash->hash_key_compare(curr->key, key) == 0) { // utente trovato
            find = 1;
            if (prev == NULL) { //se l'utente è il primo della lista di trabocco
                hash->buckets[hash_val] = curr->next;
                break;
            }
            else {
                prev->next = curr->next;
                break;
            }
        }
        else{
            prev = curr;
            curr = curr->next;
        }
    }
    doneW(hash_val);
    
    if(find == 0)   //Se l'utente non esiste
        return 1;
    
    hash_decDim(hash);  //decremento il numero di utenti registrati
    hash_disc(hash);    //decremento il numero di utenti connessi
    
    freeUsr(curr->usr); //libero la memoria dell'utente
    free(curr);
    return 0;
}


/**
 * @function hash_find
 * @brief Cerca un elemento nella tabella hash.
 *
 * @param hash Tabella hash.
 * @param key Chiave da cercare.
 * @return L'elemento in caso di successo, NULL altrimenti.
 */
hash_entry* hash_find(hash_table* hash, char* key){
    
    if(hash == NULL || key == NULL)
        return NULL;
    
    hash_entry* curr;
    unsigned int hash_val = (* hash->hash_fun)(key) % hash->nbuckets;
    
    startR(hash_val);
    for (curr=hash->buckets[hash_val]; curr != NULL; curr=curr->next){  //Scorre la lista di trabocco
        
        if ( (hash->hash_key_compare(curr->key, key))==0){ //elemento trovato
            doneR(hash_val);
            return curr;
        }
    }
    doneR(hash_val);
    return NULL;
}


/**
 * @function hash_getInfo
 * @brief Cerca l'utente nella tabella e setta i parametri fd e st (se != NULL) passati alla funzione con i
 *        valori dell'utente.
 *
 * @param ht Tabella hash degli utenti.
 * @param key Chiave dell'utente.
 * @param fd File descriptor dell'utente.
 * @param st Status dell'utente.
 * @return Setta i parametri fd e st, se != NULL, e ritorna 0 in caso di successo,1 se non trova l'utente, -1 altrimenti.
 */

int hash_getInfo(hash_table* ht,char* key,int *fd, int *st){
    
    if(ht == NULL || key == NULL){
        *fd = -1;
        *st = -1;
        return -1;
    }
    
    hash_entry *curr;
    if((curr=hash_find(ht,key)) == NULL){
        return 1;
    }
    
    if(fd != NULL){     //Se il parametro fd != NULL setta fd al file descriptor dell'utente
        startR(((* ht->hash_fun)(key)) % ht->nbuckets);
        *fd = getFd(curr->usr);
        doneR(((* ht->hash_fun)(key)) % ht->nbuckets);
    }
    if(st != NULL){ //Se il parametro st != NULL setta st allo status dell'utente
        startR(((* ht->hash_fun)(key)) % ht->nbuckets);
        *st = getStatus(curr->usr);
        doneR(((* ht->hash_fun)(key)) % ht->nbuckets);
    }
    return 0;
}


/**
 * @function hash_connect
 * @brief Segna un cliente come connesso.
 
 * @param ht Hash table dei client.
 * @param key Chiave del client da connettere.
 * @param fd Nuovo file descriptor dell'utente da connettere.
 * @return ritorna 0 in caso di successo, -1 altrimenti;
 */
int hash_connect(hash_table *ht,char* key,int fd){
    
    if(ht == NULL || key == NULL || fd < 0)
        return -1;
    
    hash_entry *f;
    int err;
    
    if( (f = hash_find(ht,key)) == NULL) //l'utente non è stato registrato
        return -1;
    else{
        startW(((* ht->hash_fun)(key)) % ht->nbuckets);
        err = connUsr(f->usr,fd);
        doneW(((* ht->hash_fun)(key)) % ht->nbuckets);
    }
    if(err == 0)	//se l'utente non era già connesso
    	hash_conn(ht); //incrementa il numero di utenti connessi
    return err;
}


/**
 * @function hash_disconnect
 * @brief Segna un cliente come disconnesso.
 
 * @param ht Hash table dei client.
 * @param key Chiave del client da connettere.
 * @return ritorna 0 in caso di successo, -1 altrimenti;
 */
int hash_disconnect(hash_table *ht,char* key){
    
    if(ht == NULL || key == NULL)
        return -1;
    
    int err;
    hash_entry *f;
    
    if( (f = hash_find(ht,key)) == NULL){ //se l'utente non è registrato
        return -1;
    }
    else{
        startW(((* ht->hash_fun)(key)) % ht->nbuckets);
        err = discUsr(f->usr);
        doneW(((* ht->hash_fun)(key)) % ht->nbuckets);
    }
    
    if(err == 0){    //se l'utente non era già disconnesso
        hash_disc(ht); //decrementa il numero di utenti connessi
        return 0;
    }
    return -1;
}


/**
 * @function hash_saveMsg
 * @brief Aggiunge un messaggio alla history di un utente.
 *
 * @param ht Hash table dei client.
 * @param key Chiave del client.
 * @param msg Messaggio da aggiungere.
 * @return ritorna 0 in caso di successo, -1 altrimenti;
 */
int hash_saveMsg(hash_table* ht,char *key, message_t* msg){
    if(ht == NULL || key == NULL || msg == NULL)
        return -1;
    
    int err = 0;
    hash_entry *elem;
    elem = hash_find(ht, key); //Cerca l'utente passato come argomento
    if(elem == NULL)    //se l'utente non è registrato
        return -1;
    startW(((* ht->hash_fun)(key)) % ht->nbuckets);
    err = addMsg(elem->usr, msg);   //Salva il messaggio nella lista dell'utente
    doneW(((* ht->hash_fun)(key)) % ht->nbuckets);

    return err;
}


/**
 * @function hash_getHist
 * @brief Ritorna tutti i messaggi presenti nella history dell'utente.
 *
 * @param ht Hash table degli utenti.
 * @param key Chiave dell'utente.
 * @return La history dell'utente in caso di successo,NULL se la lista è vuota o in caso di errore.
 */
message_t* hash_getHist(hash_table* ht, char* key, int *dim){
    if(ht == NULL || key == NULL)
        return NULL;
    
    message_t *hst;
    hash_entry *elem;
    if((elem = hash_find(ht, key)) == NULL){ //Cerca l'utente passato come argomento
    	*dim = -1;
       return NULL;
     }

    startR(((* ht->hash_fun)(key)) % ht->nbuckets);
    *dim = getHistSize(elem->usr);  //Salva in dim il numero di messaggi nella memoria dell'utente
    if(*dim <= 0){
    	doneR(((* ht->hash_fun)(key)) % ht->nbuckets);
        return NULL;
    }
    hst = getHist(elem->usr);       //Salva in hst tutti i messaggi nella lista dell'utente
    
    doneR(((* ht->hash_fun)(key)) % ht->nbuckets);
    return hst;
}

/**
 * @function hash_getAllFd
 * @brief Inserisce nell'array passato tutti i file descriptor degli utenti.
 *
 * @param ht Hash table degli utenti
 * @param arr array in cui inserire i file descriptor
 * @return ritorna il numero di file descriptor in caso di successo, -1 altrimenti
 */
int hash_getAllFd(hash_table* ht, int* arr){
    if(ht == NULL)
        return -1;
    
    hash_entry *curr;
    int count = 0,i = 0,j = 0;
    readAll(ht->nbuckets);
    while(count < ht->nentries){    //Scorre tutti gli utenti e salva il loro File Descriptor
        curr = ht->buckets[i];
        while(curr != NULL){
            arr[j] = getFd(curr->usr);
            count++;
            j++;
            curr = curr->next;
        }
        i++;
    }
    doneAllR(ht->nbuckets);
    return count;
}


/**
 * @function hash_getUSer
 * @brief ritorna la lista degli utenti registrati
 *
 * @param ht Hash Table degli utenti.
 * @param dim Variabile in cui scrivere il numero di utenti registrati.
 * @return Gli utenti registrati in caso di successo, NULL altrimenti.
 */
char* hash_getUsers(hash_table* ht,int *dim){
    if(ht == NULL)
        return NULL;
    
    int len = hash_getDim(ht),i = 0,count = 0; //len conta il numero di utenti online
    
    if(len < 0)
        return NULL;
    
    char* tmp;
    char* users = (char*) malloc((len*(MAX_NAME_LENGTH + 1)+1)*sizeof(char));
    char* check;
    
    if(users == NULL){
        perror("Allocating memory");
        exit(EXIT_FAILURE);
    }
    memset(users, 0, (len*(MAX_NAME_LENGTH + 1)+1)*sizeof(char));
    
    hash_entry *curr;
    tmp = users;
    
    readAll(ht->nbuckets);
  
    while(count < ht->nentries){    //Scorre tutte le bucket
        curr = ht->buckets[i];
        while(curr != NULL){		//scorre la lista di trabocco
            
            CHECK_NULL(check,strncpy(tmp,getNick(curr->usr),MAX_NAME_LENGTH +1),"Coping nickname");
            tmp += MAX_NAME_LENGTH;
            *tmp = '\0';    //inserisco il terminatore alla fine di ogni nickname
            tmp++;
            count++;
            curr = curr->next;
        }
        i++;
    }
    users[len*(MAX_NAME_LENGTH + 1)] = '\0';
    *dim = len;
    doneAllR(ht->nbuckets);
    
    return users;
}


/**
 * @function hash_getAllKey
 * @brief restituisce tutte le chiavi degli utenti e le scrive nel vettore passato come argomento
 *
 * @param ht Hash Table degli utenti.
 * @param vec vettore dove scrivere le chiavi
 * @return il numero di chiavi salvate
 */
int hash_getAllKey(hash_table* ht, char** vec){
    if(ht == NULL)
        return -1;
    hash_entry *curr;
    int count = 0,i = 0,j = 0; //i: indice delle bukets, j: indice del vec
    
    readAll(ht->nbuckets);
    while(count < ht->nentries){    //Scorre tutti gli utenti e salva la loro chiave
        curr = ht->buckets[i];
        while(curr != NULL){
            strcpy(vec[j], getNick(curr->usr));
            count++;
            j++;
            curr = curr->next;
        }
        i++;
    }
    doneAllR(ht->nbuckets);
    return count;
}

/**
 * @function hash_fdDisconnect
 * @brief disconnette l'utente corrispondete al file descriptor passato come argomento
 *
 * @param ht Hash Table degli utenti.
 * @param fd File Descriptor dell'utente da eliminare
 * @return 0 in caso di successo, -1 altrimenti
 */
int hash_fdDisconnect(hash_table* ht, int fd){
    hash_entry *bucket, *curr;
    
    readAll(ht->nbuckets);
    
    for(int i=0; i<ht->nbuckets; i++) { //cerca l'utente in base al file descriptor
        bucket = ht->buckets[i];
        for(curr=bucket; curr!=NULL; ) {
            if(curr->usr->fd_u == fd){
                doneAllR(ht->nbuckets);
                hash_disconnect(ht,curr->key);
                return 0;
            }
            curr=curr->next;
        }
    }
    doneAllR(ht->nbuckets);
    return -1;
}

/**
 * @function hash_destroy
 * @brief Cancella la tabella hash e libera la memoria.
 *
 * @param hash Tabella hash.
 * @return 0 in caso di successo, -1 altrimenti.
 */
int hash_destroy(hash_table* hash){
	
	if(hash == NULL)
		return -1;
	
    hash_entry *bucket, *curr, *next;
    
    writeAll(hash->nbuckets);	
   
    for (int i=0; i<hash->nbuckets; i++) {      //Scorre tutte le bucket
        bucket = hash->buckets[i];
        for (curr=bucket; curr!=NULL; ) {  //scorre la lista di trabocco
            next=curr->next;
            free(curr->key);
            if(curr->usr) freeUsr(curr->usr);
            free(curr->usr);
            curr=next;
        }
    }
    
    doneAllW(hash->nbuckets); 
    
    for(int i=0; i< hash->nbuckets; i++){
        free(hash->buckets[i]);
    }
    if(hash->buckets)
        free(hash->buckets);
    
    freeMtx();      //Libera la memoria della mutua esclusione
    
    if(hash){
    	free(hash);
    	hash = NULL;
    }
    
    return 0;
}


/**
 * @function hash_print
 * @brief Stampa tutti gli elementi della tabella hash e relativi attributi.
 *
 * @param hash Tabella hash.
 * @param out File descriptor del file in cui stampare.
 */
void hash_print(hash_table* hash, FILE* out){
    hash_entry *bucket, *curr;
    int i;
    
    fprintf(out,"\n-------------------------\n");
    fprintf(out,"HASH TABLE CLIENT\n");
    
    readAll(hash->nbuckets);
    for(i=0; i<hash->nbuckets; i++) {
        bucket = hash->buckets[i];
        for(curr=bucket; curr!=NULL; ) {
            fprintf(out,"\nKEY: %s\nUSER: %s\nFD: %d\nSTATUS: %d\n",curr->key,curr->usr->nick,curr->usr->fd_u, curr->usr->st);
            curr=curr->next;
        }
    }
    doneAllR(hash->nbuckets);
    fprintf(out,"\n-------------------------\n");
   
    return;
    
}

///------------- Funzioni per la gestione in ME di nentries e connected ----------------

/**
 * @function hash_getDim
 * @brief   Ritorna il numero di utenti nella tabella.
 *
 * @param ht Hash table degli utenti.
 * @return Ritorna il numero di utenti nella tabella.
 */
int hash_getDim(hash_table* ht){
	if(ht == NULL) return -1;
    pthread_mutex_lock(&entry_mtx);
    int n = ht->nentries;
    pthread_mutex_unlock(&entry_mtx);
    return n;
}


/**
 * @function hash_incDim
 * @brief  Incrementa il numero di utenti inseriti.
 *
 * @param ht Hash table degli utenti.
 */
void hash_incDim(hash_table *ht){
	if(ht == NULL) return ;
    pthread_mutex_lock(&entry_mtx);
    ht->nentries++;
    pthread_mutex_unlock(&entry_mtx);
}


/**
 * @function hash_decDim
 * @brief  decrementa il numero di utenti inseriti.
 *
 * @param ht Hash table degli utenti.
 */
void hash_decDim(hash_table *ht){
	if(ht == NULL) return ;
    pthread_mutex_lock(&entry_mtx);
    ht->nentries--;
    pthread_mutex_unlock(&entry_mtx);
}


/**
 * @function hash_getConn
 * @brief   Ritorna il numero di utenti connessi.
 *
 * @param ht Hash table degli utenti.
 * @return Ritorna il numero di utenti connessi.
 */
int hash_getConn(hash_table* ht){
	if(ht == NULL) return -1;
    pthread_mutex_lock(&conn_mtx);
    int n = ht->connected;
    pthread_mutex_unlock(&conn_mtx);
    return n;
}


/**
 * @function hash_conn
 * @brief  Incrementa il numero di utenti connessi.
 *
 * @param ht Hash table degli utenti.
 */
void hash_conn(hash_table *ht){
	if(ht == NULL) return ;
    pthread_mutex_lock(&conn_mtx);
    ht->connected+= 1;
    pthread_mutex_unlock(&conn_mtx);
}


/**
 * @function hash_disc
 * @brief  decrementa il numero di utenti connessi.
 *
 * @param ht Hash table degli utenti.
 */
void hash_disc(hash_table *ht){
	if(ht == NULL) return ;
    pthread_mutex_lock(&conn_mtx);
    ht->connected-= 1;
    pthread_mutex_unlock(&conn_mtx);
}
///-------------------------------------------------


///------------- Funzioni per la Mutua Esclusione ----------------


/**
 * @function setMtx
 * @brief alloca e inizializza le variabili condizionali e di mutua esclusione e gli array dei lettori e scrittori
 *
 * @param ht Hash Table degli utenti
 * @return 0 in caso di successo, -1 altrimenti
 */
int setMtx(hash_table* ht){
	if(ht == NULL) return -1;
	
    int space = 0,err = 0,err1 = 0, err2 = 0;
    if(ht->nbuckets < CRITICAL_VALUE ){   //setto la variabile dim in base al numero di bucket
        dim = ht->nbuckets;
        space = 1;
    }
    else{
        dim = (int) ht->nbuckets/3;
        space = dim;
    }
    //inizializzo gli array di mutex e delle variabili condizionali
    CHECK_NULL(mtx,(pthread_mutex_t*) malloc(space*sizeof(pthread_mutex_t)),"Allocating mtx");
    CHECK_NULL(rGo,(pthread_cond_t*) malloc(space*sizeof(pthread_cond_t)),"Allocating cond");
    CHECK_NULL(wGo,(pthread_cond_t*) malloc(space*sizeof(pthread_cond_t)),"Allocating cond");
    
    memset(mtx,0,space*sizeof(pthread_mutex_t));
    memset(rGo,0,space*sizeof(pthread_cond_t));
    memset(wGo,0,space*sizeof(pthread_cond_t));
    
    for (int i = 0; i < space ; i++) {  //inizializzo tutte le mutex e le variabili condizionali
        err =pthread_mutex_init(&mtx[i],NULL);
        err1 =pthread_cond_init(&rGo[i],NULL);
        err2 =pthread_cond_init(&wGo[i],NULL);
        
        if(err != 0 || err1 != 0 || err2 != 0)
            exit(EXIT_FAILURE);
    }
    
    //alloco gli array degli utenti attivi o in attesa
    CHECK_NULL(activeR, (int*)malloc(space*(sizeof(int))),"Allocating array");
    CHECK_NULL(activeW, (int*)malloc(space*(sizeof(int))),"Allocating array");
    CHECK_NULL(waitingR, (int*)malloc(space*(sizeof(int))),"Allocating array");
    CHECK_NULL(waitingW, (int*)malloc(space*(sizeof(int))),"Allocating array");
    
    memset(activeR,0,space*(sizeof(int)));
    memset(activeW,0,space*(sizeof(int)));
    memset(waitingR,0,space*(sizeof(int)));
    memset(waitingW,0,space*(sizeof(int)));
    
    CHECK_NEG(err,initialize_mtx(space),"Initializing array");
    
    return 0;
}


/**
 * @function freeMtx
 * @brief libera la memoria relativa alle variabili condizionali e di mutua esclusione
 */
void freeMtx(){
	
    //Libero la memoria degli array
    free(mtx);
    free(rGo);
    free(wGo);
    free(activeW);
    free(activeR);
    free(waitingR);
    free(waitingW);
    
	return ;
}


/**
 * @function initializa_mtx
 * @brief inizializza gli array dei lettori e scrittori
 *
 * @param dim numero di elementi dell'array
 * @return 0 in caso di successo, -1 altrimenti
 */
int initialize_mtx(int dim){
	if(dim < 0) return -1;
	
    for(int i = 0; i < dim;i++){     //Inizializzo tutti gli array degli utenti attivi
        activeW[i] = 0;
        activeR[i] = 0;
        waitingW[i] = 0;
        waitingR[i] = 0;
    }
    return 0;
}


/**
 * @function value
 * @brief calcola l'indice da usare per la mutua esclusione a partire dall' hash_value
 *
 * @param hash_val valore hash di cui calcolare l'indice
 * @return indice da usare per la mutua esclusione a partire dall' hash_value
 */
int value(int hash_val){
    return (int) (hash_val/dim);
}


/**
 * @function startR
 * @brief incrementa il numero di lettori di un dato blocco di entrate della tabella hash
 *
 * @param hash_val valore hash dell' entrata della tabella da leggere
 * @return 0 in caso di successo, -1 altrimenti
 */
int startR(int hash_val){
    int num = value(hash_val);
    pthread_mutex_lock(&mtx[num]);
    waitingR[num]++;
    while(activeW[num] > 0 || waitingW[num] > 0){	//se uno o più scrittori stanno scrivendo o aspettando, aspetto
        pthread_cond_wait(&rGo[num],&mtx[num]);
    }
    waitingR[num]--;
    activeR[num]++;
    pthread_mutex_unlock(&mtx[num]);
    return 0;
}

/**
 * @function doneR
 * @brief decrementa il numero di lettori di un dato blocco di entrate della tabella hash
 *
 * @param hash_val valore hash dell' entrata della tabella da leggere
 * @return 0 in caso di successo, -1 altrimenti
 */
int doneR(int hash_val){
    int num = value(hash_val);
    pthread_mutex_lock(&mtx[num]);
    activeR[num]--;
    if(activeR[num] == 0 && waitingW[num] > 0)	//se non ci sono lettori attivi e ci sono scrittori in attesa li sveglio
        pthread_cond_signal(&wGo[num]);
    pthread_mutex_unlock(&mtx[num]);
    return 0;
}

/**
 * @function startW
 * @brief incrementa il numero di scrittori di un dato blocco di entrate della tabella hash
 *
 * @param hash_val valore hash dell' entrata della tabella da scrivere
 * @return 0 in caso di successo, -1 altrimenti
 */
int startW(int hash_val){
    int num = value(hash_val);
    pthread_mutex_lock(&mtx[num]);
    waitingW[num]++;
    while(activeW[num] > 0 || activeR[num] > 0){ //aspetta il suo turno per scrivere
        pthread_cond_wait(&wGo[num],&mtx[num]);
    }
    waitingW[num]--;
    activeW[num]++;
    pthread_mutex_unlock(&mtx[num]);
    
    return 0;
}

/**
 * @function doneW
 * @brief decrementa il numero di scrittori di un dato blocco di entrate della tabella hash
 *
 * @param hash_val valore hash dell' entrata della tabella da scrivere
 * @return 0 in caso di successo, -1 altrimenti
 */
int doneW(int hash_val){
    int num = value(hash_val);
    pthread_mutex_lock(&mtx[num]);
    activeW[num]--;
    if( activeW[num] > 0){	//controllo se per errore stanno scrivendo più writers
        printf("ERRORE TROPPI WRITERS\n");
        exit(EXIT_FAILURE);
    }
    if(waitingW[num] > 0)	//se un writer in attesa lo sveglio
        pthread_cond_signal(&wGo[num]);
    else{					//altrimenti sveglio tutti gli scrittori
        pthread_cond_broadcast(&rGo[num]);
    }
    pthread_mutex_unlock(&mtx[num]);
    return 0;
}


/**
 * @function readAll
 * @brief incrementa il numero di lettori di ogni blocco di entrate della tabella
 *
 * @param buck numero di bucket della tabella
 * @return 0 in caso di successo, -1 altrimenti
 */
int readAll(int buck){
    int inc = buck/dim;
    for(int i = 0; i < dim; i = (i+inc)*dim){	//chiedo di poter leggere ogni bucket
        startR(i);
    }
    return 0;
}

/**
 * @function writeAll
 * @brief incrementa il numero di scrittori di ogni blocco di entrate della tabella
 *
 * @param buck numero di bucket della tabella
 * @return 0 in caso di successo, -1 altrimenti
 */
int writeAll(int buck){
    int inc = buck/dim;
    for(int i = 0; i < dim; i = (i+inc)*dim){	//chiedo di poter scrivere ogni bucket
        startW(i);
    }
    return 0;
}

/**
 * @function donAllR
 * @brief decrementa il numero di lettori di ogni blocco di entrate della tabella
 *
 * @param buck numero di bucket della tabella
 * @return 0 in caso di successo, -1 altrimenti
 */
int doneAllR(int buck){
    int inc = buck/dim;
    for(int i = 0; i < dim; i = (i+inc)*dim){
        doneR(i);
    }
    return 0;
}

/**
 * @function donAllW
 * @brief decrementa il numero di scrittori di ogni blocco di entrate della tabella
 *
 * @param buck numero di bucket della tabella
 * @return 0 in caso di successo, -1 altrimenti
 */
int doneAllW(int buck){
    int inc = buck/dim;
    for(int i = 0; i < dim; i = (i+inc)*dim){
        doneW(i);
    }
    return 0;
}




///-------------------------------------------------






