///file con le informazioni di configurazione lette dal file chatty.conf
/**
 * @author Lorenzo Arcidiacono, 534235, arci0066@gmail.com
 * Si dichiara che il programma è in ogni sua parte opera originale dell'autore.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#if !defined(CONF_H_)
#define CONF_H_

/** @brief Variabili di configurazione
 */
struct conf{
	long int conn ;
	long int pool;
	long int msg_dim;
	long int file ;
	long int hist;

	char* path;
	char* dir;
	char* stat_file;
};

//-------- funzioni ---------

/**
 * @function init
 * @brief inizializza tutte le variabili di configurazione in base al file
 * @warning Chiamare questa funzione all'avvio del server.
 *
 * @param fd File Descriptor del file di configurazione.
 * @param c  Struct dei valori da inizializzare.
 *
 * @return 0 in caso di successo,
 *         -1 in caso di errore
 */
int init(FILE* fd, struct conf *c );


/**
 * @function clearAllConf
 * @brief Libera lo spazio in memoria occupato. Da chiamare alla chiusura
 *        del server.
 *
 * @param c Struct dei valori da liberare.
 *
 */
void clearAllConf(struct conf* c);

#endif /* CONF_H_ */
