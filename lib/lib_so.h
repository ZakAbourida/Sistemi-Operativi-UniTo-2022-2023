#include <errno.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define BZERO(x, x_size) memset(x, 0, x_size)
#define VIA_SEM 0
#define ANNUNCI_SEM 1
#define DUMP_PORTI_SEM 2
#define DUMP_NAVI_SEM 3
#define DUMP_MERCI_SEM 4
#define LIST_NAVI_SEM 5
#define NUM_SEMS 6
#define MAX_MESSAGE_SIZE 10
#define H_IN_NSEC 42000000
#define TEST_ERROR                                                          \
    if (errno) {                                                            \
        dprintf(STDERR_FILENO, "%s:%d: PID=%5d: Error %d (%s)\n", __FILE__, \
                __LINE__, getpid(), errno, strerror(errno));                \
    }                                                                       \
    errno = 0;

typedef struct {
    int pos_x, pos_y;
} Posizione;

typedef struct {
    int SO_NAVI, SO_PORTI, SO_MERCI, SO_SIZE, SO_MIN_VITA, SO_MAX_VITA, SO_FILL,
        SO_LATO, SO_SPEED, SO_CAPACITY, SO_BANCHINE, SO_LOADSPEED, SO_DAYS,
        SO_STORM_DURATION, SO_SWELL_DURATION, SO_MALESTORM;
} Conf_values;

union semun {
    int val;               /* Value for SETVAL */
    struct semid_ds *buf;  /* Buffer for IPC_STAT, IPC_SET */
    unsigned short *array; /* Array for GETALL, SETALL */
    struct seminfo *__buf; /* Buffer for IPC_INFO
                              (Linux-specific) */
};

typedef struct {
    int type;
    int qta; /*     in lotti*/
    int scadenza;
    int stato_merce; /*0: in_porto 1: in_nave 2: consegnata 3: scad_porto 4: scad_nave */
} Merce;

typedef struct {
    int porto;
    int qta; /*   in lotti*/
    int merce_type;
} Domanda;

typedef struct {
    int porto;
    int qta; /*   in lotti*/
    int merce_type;
    int scadenza;
} Offerta;

typedef struct {
    int merce_type;
    int merce_qta; /* in lotti */
    Posizione porto_domanda;
    Posizione porto_offerta;
    int id_porto_domanda;
    int id_porto_offerta;
    int d_index;
    int o_index;
} Comanda;

typedef struct {
    long message_type;
    int payload[2];
} Message;

typedef enum {
    IN_VIAGGIO,
    IN_PORTO
} StatoNave;

typedef struct {
    int id;
    pid_t pid;
    StatoNave stato;
} Nave;

/** DESCRIZIONE MEMORIE CONDIVISE: 
 *      - CONFIG: molto intuitivo
 * 
 *      - DUMP NAVI:    array di interi ( tipo heap )
 *                      [0] = navi in mare con carico
 *                      [1] = navi in mare senza carico
 *                      [2] = navi in porto carico
 *                      [3] = navi in porto scarico
 *                      [4] = navi colpite da tempesta
 *                      [5] = navi affondate da maelstrom
 * 
 *      - DUMP PORTI:   array di interi ( tipo heap )
 *                      [0] = porto pos_x
 *                      [1] = porto pos_y
 *                      [2] = qta_presente
 *                      [3] = qta_spedita
 *                      [4] = qta_ricevuta
 *                      [5] = banchine_tot
 *                      [6] = banchine_occ
 * 
 *      - DUMP MERCI:   array di interi ( tipo heap )
 *                      [0] = merce_type
 *                      [1] = qta_porto
 *                      [2] = qta_nave
 *                      [3] = qta_consegnata
 *                      [4] = qta_scaduta_porto
 *                      [5] = qta_scaduta_nave
 * 
 *      - DOMANDE: array di struct Domanda
 *                      .porto
 *                      .qta
 *                      .merce_type
 * 
 *      - OFFERTE: array di struct Offerta
 *                      .porto
 *                      .qta
 *                      .merce_type
 *                      .scadenza
 * 
 *      - REGISTRO: array di int ( grande quanto il numero di merci )
 *                               ( ciascuno indica la grandezza del lotto di quella merce )
 *                      
 *      - METEO: array di int ( grande quanto il numero di porti )
 * 
 *      - LISTA NAVI: array di struct Nave
 * 
 *      
 * 
*/
