#include "../lib/lib_so.h"

key_t sem_id, msg_id, dump_navi_key, dump_porti_key, dump_merci_key, config_key,
    domanda_key, offerta_key, registro_key;
Conf_values *config_ptr;
int *dump_porti, *dump_merci, *dump_navi, *registro, banchine, id, i, resto;
Domanda *domande;
Offerta *offerte;
Posizione my_pos;
struct sembuf sops;
int status; /*0 = porto operativo, 1 = porto non operativo(mareggiata in corso)*/
sigset_t mask;

void safeWait(int sec, long nsec) {
    struct timespec waitT, Trimasto;
    int st;

    waitT.tv_sec = sec;
    waitT.tv_nsec = nsec;

    do {
        errno = 0;
        st = nanosleep(&waitT, &Trimasto);
        if (st == 0)
            break;
        waitT = Trimasto;
    } while (st == -1 && errno == EINTR);
    errno = 0;
}

void signal_handler(int sig) {
    struct timespec tempo;
    /*arriva il messaggio della mareggiata*/
    if (sig == SIGUSR1) {
        if (status != 1) {
            status = 1;
            sigprocmask(SIG_BLOCK, &mask, NULL);

            tempo.tv_sec = 0;
            tempo.tv_nsec = H_IN_NSEC * config_ptr->SO_SWELL_DURATION;

            safeWait(tempo.tv_sec, tempo.tv_nsec);

            sigprocmask(SIG_UNBLOCK, &mask, NULL);
            status = 0;
            TEST_ERROR;
        }
        return;
    }

    exit(EXIT_SUCCESS);
}

void initializeResourcePorto(char const *argv[]) {

    sem_id = atoi(argv[1]);
    msg_id = atoi(argv[2]);
    dump_navi_key = atoi(argv[3]);
    dump_porti_key = atoi(argv[4]);
    dump_merci_key = atoi(argv[5]);
    config_key = atoi(argv[6]);
    domanda_key = atoi(argv[7]);
    offerta_key = atoi(argv[8]);
    registro_key = atoi(argv[9]);

    id = (atoi(argv[10]));

    /*ATTACHMENT SEGMENTI MEMORIA CONDIVISA*/
    config_ptr = (Conf_values *)shmat(config_key, NULL, SHM_RDONLY);
    TEST_ERROR;

    dump_navi = (int *)shmat(dump_navi_key, NULL, 0);
    TEST_ERROR;

    dump_porti = (int *)shmat(dump_porti_key, NULL, 0);
    TEST_ERROR;

    dump_merci = (int *)shmat(dump_merci_key, NULL, 0);
    TEST_ERROR;

    domande = (Domanda *)shmat(domanda_key, NULL, 0);
    TEST_ERROR;

    offerte = (Offerta *)shmat(offerta_key, NULL, 0);
    TEST_ERROR;

    registro = (int *)shmat(registro_key, NULL, 0);
    TEST_ERROR;

    /*inizializzo la maschera*/
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGALRM);
    sigaddset(&mask, SIGUSR2);
}

void initializePorto() {
    /*POSIZIONE*/
    int ip;
    ip = 0;
    switch (id) {
    case 0:
        my_pos.pos_x = 0;
        my_pos.pos_y = 0;
        break;
    case 1:
        my_pos.pos_x = config_ptr->SO_LATO;
        my_pos.pos_y = 0;
        break;
    case 2:
        my_pos.pos_x = 0;
        my_pos.pos_y = config_ptr->SO_LATO;
        break;
    case 3:
        my_pos.pos_x = config_ptr->SO_LATO;
        my_pos.pos_y = config_ptr->SO_LATO;
        break;

    default:
        my_pos.pos_x = rand() % config_ptr->SO_LATO;
        my_pos.pos_y = rand() % config_ptr->SO_LATO;

        while (ip < config_ptr->SO_PORTI) {
            if (dump_porti[ip * 7 + 0] == my_pos.pos_x &&
                dump_porti[ip * 7 + 1] == my_pos.pos_y) {
                my_pos.pos_x = rand() % config_ptr->SO_LATO;
                my_pos.pos_y = rand() % config_ptr->SO_LATO;
                ip = 0;
            } else
                ip++;
        }
        break;
    }

    /* BANCHINE*/
    banchine = rand() % config_ptr->SO_BANCHINE + 1;
}

void initializeDataPorto() {
    status = 0;

    sops.sem_num = DUMP_PORTI_SEM;
    sops.sem_op = -1;
    semop(sem_id, &sops, 1);

    /* aggiorna i suoi valori*/

    dump_porti[id * 7 + 0] = my_pos.pos_x; /* porto(x) */
    dump_porti[id * 7 + 1] = my_pos.pos_y; /* porto(y) */
    dump_porti[id * 7 + 3] = 0;            /* qta_spedita */
    dump_porti[id * 7 + 4] = 0;            /* qta_ricevuta */
    dump_porti[id * 7 + 5] = banchine;     /* banchine_tot */
    dump_porti[id * 7 + 6] = 0;            /* banchine_occ */

    sops.sem_num = DUMP_PORTI_SEM;
    sops.sem_op = 1;
    semop(sem_id, &sops, 1);
}

int main(int argc, char const *argv[]) {
    int j, merce_type, merce_qty, id_nave, result;
    struct sigaction saPorto;
    Message message;

    srand(time(NULL) ^ getpid());

    BZERO(&saPorto, sizeof(saPorto));
    saPorto.sa_handler = signal_handler;

    sigaction(SIGINT, &saPorto, NULL);
    sigaction(SIGTERM, &saPorto, NULL);
    sigaction(SIGALRM, &saPorto, NULL);
    sigaction(SIGUSR1, &saPorto, NULL);

    initializeResourcePorto(argv);

    initializePorto();

    initializeDataPorto();

    sops.sem_num = NUM_SEMS + id;
    sops.sem_op = banchine;
    semop(sem_id, &sops, 1);

    sops.sem_num = VIA_SEM;
    sops.sem_op = -1;
    semop(sem_id, &sops, 1);

    /*Esecuzione:*/
    while (1) {
        /** La nave si occupa di bloccare e sbloccare le bachine. Il porto si
         * gestisce la merce che è stata "lasciata sulla banchina" */

        /**
         * MESSAGGIO:
         *          - message_type --> id del porto +1
         *          - contenuto[0] --> tipo della merce
         *          - contenuto[1] --> quantità della merce
         */

        do {
            result = msgrcv(msg_id, &message, sizeof(Message), id + 1, 0);
        } while (result == -1 && errno == EINTR);

        merce_type = message.payload[0];
        merce_qty = message.payload[1]; /* qta non in lotti ma in ton. (già convertita)*/

        sops.sem_num = DUMP_PORTI_SEM;
        sops.sem_op = -1;
        semop(sem_id, &sops, 1);

        dump_porti[id * 7 + 2] += merce_qty; /*          ++QTA_PRESENTE*/

        if (merce_qty > 0) {                     /* SI: sto facendo richiesta, NO: sto facendo offerta*/
            dump_porti[id * 7 + 4] += merce_qty; /*      ++QTA_RICEVUTA*/
        } else {
            dump_porti[id * 7 + 3] += -merce_qty; /*     ++QTA_SPEDITA*/
        }

        sops.sem_num = DUMP_PORTI_SEM;
        sops.sem_op = 1;
        semop(sem_id, &sops, 1);
    }

    signal_handler(SIGINT);
}