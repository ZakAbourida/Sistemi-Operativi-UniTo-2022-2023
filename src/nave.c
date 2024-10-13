#include "../lib/lib_so.h"

key_t sem_id, msg_id, dump_porti_key, dump_merci_key, dump_navi_key, config_key,
    domanda_key, offerta_key, porto_msq_id, registro_key, navi_shm_k;
Conf_values *config_ptr;
int *dump_porti, *dump_merci, *dump_navi, *registro_ptr, i, id, carico, vel,
    cap_massima, vel_caricamento;
Domanda *domande;
Offerta *offerte;
struct sembuf sops;
Posizione current_pos, dest;
Comanda comanda;
Nave *arrNave;

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
    int s;

    if (sig == SIGUSR1) {
        /*tempesta*/
        fprintf(stderr, "NAVE | %d | %d |: rallentata da una tempesta!\n",
                id, getpid());
        sops.sem_num = DUMP_NAVI_SEM;
        sops.sem_op = -1;
        semop(sem_id, &sops, 1);

        dump_navi[4]++; /* ++NAVI RALLENTATE DA TEMPESTE*/

        sops.sem_num = DUMP_NAVI_SEM;
        sops.sem_op = 1;
        semop(sem_id, &sops, 1);

        tempo.tv_sec = 0;
        tempo.tv_nsec = H_IN_NSEC * config_ptr->SO_STORM_DURATION;
        return;

    } else if (sig == SIGUSR2) {
        /*maelstrom*/
        fprintf(stderr, "NAVE | %d | %d |: affondata da maelstrom\n",
                id, getpid());

        if (carico == 0) {
            sops.sem_num = ANNUNCI_SEM;
            sops.sem_op = -1;
            sops.sem_flg = 0;
            semop(sem_id, &sops, 1);

            domande[comanda.d_index].qta += comanda.merce_qta;
            offerte[comanda.o_index].qta += comanda.merce_qta;

            sops.sem_num = ANNUNCI_SEM;
            sops.sem_op = 1;
            sops.sem_flg = 0;
            semop(sem_id, &sops, 1);

            sops.sem_num = DUMP_NAVI_SEM;
            sops.sem_op = -1;
            semop(sem_id, &sops, 1);

            dump_navi[1]--; /* --NAVE IN MARE SCARICA*/
            dump_navi[5]++; /* ++NAVI AFFONDATE DA MAELSTORM*/

            sops.sem_num = DUMP_NAVI_SEM;
            sops.sem_op = 1;
            semop(sem_id, &sops, 1);

        } else {
            sops.sem_num = ANNUNCI_SEM;
            sops.sem_op = -1;
            sops.sem_flg = 0;
            semop(sem_id, &sops, 1);

            domande[comanda.d_index].qta += comanda.merce_qta;

            sops.sem_num = ANNUNCI_SEM;
            sops.sem_op = 1;
            sops.sem_flg = 0;
            semop(sem_id, &sops, 1);

            sops.sem_num = DUMP_NAVI_SEM;
            sops.sem_op = -1;
            semop(sem_id, &sops, 1);

            dump_navi[0]--; /* --NAVE IN MARE CARICA*/
            dump_navi[5]++; /* ++NAVI AFFONDATE DA MAELSTORM*/

            sops.sem_num = DUMP_NAVI_SEM;
            sops.sem_op = 1;
            semop(sem_id, &sops, 1);
        }
    }

    exit(EXIT_SUCCESS);
}

Posizione find_porto(int id) {
    Posizione ret;
    ret.pos_x = dump_porti[id * 7 + 0];
    ret.pos_y = dump_porti[id * 7 + 1];
    return ret;
}

Comanda ricerca_annuncio(int retryes) {
    Offerta tmp_off;
    Domanda tmp_dom;
    Comanda ret;
    size_t i;
    int mt, op, dp;

    tmp_off.merce_type = -1;
    tmp_dom.merce_type = -1;

    if (retryes == 100) {
        ret.merce_qta = 0;
        ret.id_porto_domanda = 0;
        ret.id_porto_offerta = 0;
        return ret;
    }

    mt = rand() % config_ptr->SO_MERCI; /* tipo merce estratto a random */
    dp = rand() % (config_ptr->SO_PORTI / 2 +
                   config_ptr->SO_PORTI % 2) +
         config_ptr->SO_PORTI / 2;            /* porto domanda estratto a random */
    op = rand() % (config_ptr->SO_PORTI / 2); /* porto offerta estratto a random */

    /*cerco un offerta corrispondente alla domanda*/
    for (i = 0;
         i < (config_ptr->SO_MERCI * (config_ptr->SO_PORTI / 2) * config_ptr->SO_DAYS);
         i++) {
        if (offerte[i].qta > 0 && offerte[i].scadenza > 0 &&
            offerte[i].merce_type == mt && offerte[i].porto == op) { /* offerta trovata*/
            tmp_off = offerte[i];
            ret.o_index = i;
            break;
        }
    }

    if (tmp_off.merce_type == -1) {
        sops.sem_num = ANNUNCI_SEM;
        sops.sem_op = 1;
        semop(sem_id, &sops, 1);

        sops.sem_num = ANNUNCI_SEM;
        sops.sem_op = -1;
        semop(sem_id, &sops, 1);

        return ricerca_annuncio(retryes++);
    }

    /*cerco una domanda libera*/
    for (i = 0;
         i < ((config_ptr->SO_PORTI / 2 + config_ptr->SO_PORTI % 2) * config_ptr->SO_MERCI);
         i++) {
        if (domande[i].merce_type == tmp_off.merce_type &&
            domande[i].qta > 0 && domande[i].porto == dp) { /* domanda trovata */
            tmp_dom = domande[i];
            ret.d_index = i;
            break;
        }
    }

    if (tmp_dom.merce_type == -1) {
        sops.sem_num = ANNUNCI_SEM;
        sops.sem_op = 1;
        semop(sem_id, &sops, 1);

        sops.sem_num = ANNUNCI_SEM;
        sops.sem_op = -1;
        semop(sem_id, &sops, 1);
        return ricerca_annuncio(retryes++);
    }

    /* domanda - offerta trovate */

    /* composizione della comanda */
    ret.merce_type = tmp_dom.merce_type;
    ret.id_porto_domanda = tmp_dom.porto;
    ret.id_porto_offerta = tmp_off.porto;
    ret.merce_qta = tmp_off.qta;

    if (ret.merce_qta > tmp_off.qta) {
        ret.merce_qta = tmp_off.qta;
    }
    if (ret.merce_qta > (cap_massima / registro_ptr[ret.merce_type])) { /* x i lotti max trasportabili */
        ret.merce_qta = (cap_massima / registro_ptr[ret.merce_type]);
    }

    ret.porto_domanda = find_porto(tmp_dom.porto);
    ret.porto_offerta = find_porto(tmp_off.porto);

    domande[ret.d_index].qta -= ret.merce_qta;
    offerte[ret.o_index].qta -= ret.merce_qta;
    return ret;
}

/*metodo nanosleep per percorso nave*/
void sleep_percorso(Posizione pos_nave, Posizione pos_porto, int speed) {
    float dx, dy;
    unsigned long int nsec;
    struct timespec tempo;

    dx = pos_nave.pos_x - pos_porto.pos_x;
    dy = pos_nave.pos_y - pos_porto.pos_y;
    nsec = (unsigned long int)(sqrt(dx * dx + dy * dy) / speed) * 1e9;

    tempo.tv_sec = (time_t)nsec / 1e9;
    tempo.tv_nsec = (long)nsec % 100000000;

    safeWait(tempo.tv_sec, tempo.tv_nsec);

    errno = 0;
}

void sleep_carico_scarico(int quantita, int loadspeed) {
    int qta;
    unsigned long int nsec;
    struct timespec tempo;

    if (quantita < 0) {
        qta = quantita * -1;
    } else {
        qta = quantita;
    }

    nsec = (unsigned long int)(qta / loadspeed) * 1e9;
    tempo.tv_sec = (time_t)nsec / 1e9;
    tempo.tv_nsec = (long)nsec % 100000000;

    safeWait(tempo.tv_sec, tempo.tv_nsec);
    errno = 0;
}

void initializeResourcesNave(char const *argv[]) {
    sem_id = atoi(argv[1]);
    msg_id = atoi(argv[2]);
    dump_navi_key = atoi(argv[3]);
    dump_porti_key = atoi(argv[4]);
    dump_merci_key = atoi(argv[5]);
    config_key = atoi(argv[6]);
    domanda_key = atoi(argv[7]);
    offerta_key = atoi(argv[8]);
    registro_key = atoi(argv[9]);
    navi_shm_k = atoi(argv[10]);
    id = (atoi(argv[11]));

    /*ATTACHMENT MEMORIA CONDIVISA*/
    config_ptr = (Conf_values *)shmat(config_key, NULL, SHM_RDONLY);
    TEST_ERROR;

    dump_navi = (int *)shmat(dump_navi_key, NULL, 0);
    TEST_ERROR;

    dump_porti = shmat(dump_porti_key, NULL, 0);
    TEST_ERROR;

    dump_merci = shmat(dump_merci_key, NULL, 0);
    TEST_ERROR;

    domande = (Domanda *)shmat(domanda_key, NULL, 0);
    TEST_ERROR;

    offerte = (Offerta *)shmat(offerta_key, NULL, 0);
    TEST_ERROR;

    registro_ptr = shmat(registro_key, NULL, 0);
    TEST_ERROR;

    arrNave = shmat(navi_shm_k, NULL, 0);
    TEST_ERROR;
}

void initializeNave() {
    /*inizializzazione cose*/
    current_pos.pos_x = rand() % config_ptr->SO_LATO;
    current_pos.pos_y = rand() % config_ptr->SO_LATO;
    vel = config_ptr->SO_SPEED;
    vel_caricamento = config_ptr->SO_LOADSPEED;
    cap_massima = config_ptr->SO_CAPACITY;
    carico = 0;
}

int main(int argc, char const *argv[]) {
    int sig, j, id_dest;
    Posizione porto_O, porto_D;
    sigset_t set;
    struct sigaction saNave;
    Message messaggio;

    BZERO(&saNave, sizeof(saNave));
    saNave.sa_handler = signal_handler;

    sigaction(SIGINT, &saNave, NULL);
    sigaction(SIGTERM, &saNave, NULL);
    sigaction(SIGALRM, &saNave, NULL);
    sigaction(SIGUSR1, &saNave, NULL);
    sigaction(SIGUSR2, &saNave, NULL);

    srand(time(NULL) ^ getpid());

    initializeResourcesNave(argv);

    initializeNave();

    sops.sem_num = VIA_SEM;
    sops.sem_op = -1;
    semop(sem_id, &sops, 1);

    while (1) {

        /* Ricerca di un annuncio*/
        sops.sem_num = ANNUNCI_SEM;
        sops.sem_op = -1;
        sops.sem_flg = 0;
        semop(sem_id, &sops, 1);

        comanda = ricerca_annuncio(0);

        sops.sem_num = ANNUNCI_SEM;
        sops.sem_op = 1;
        semop(sem_id, &sops, 1);

        /* se ho merce == 0 o porto_domanda == porto_offerta rifaccio ciclo*/
        if (comanda.merce_qta != 0) {
            porto_D = comanda.porto_domanda;
            porto_O = comanda.porto_offerta;

            /* viaggio verso porto Offerta*/
            sleep_percorso(current_pos, porto_O, vel_caricamento);
            current_pos = porto_O;

            sops.sem_num = LIST_NAVI_SEM;
            sops.sem_op = -1;
            semop(sem_id, &sops, 1);

            arrNave[id].stato = IN_PORTO;

            sops.sem_num = LIST_NAVI_SEM;
            sops.sem_op = 1;
            semop(sem_id, &sops, 1);

            /* arrivati al porto dove bisogna caricare*/
            /* attracco e carico della merce */
            sops.sem_num = NUM_SEMS + comanda.id_porto_offerta;
            sops.sem_op = -1;
            semop(sem_id, &sops, 1);

            sops.sem_num = DUMP_NAVI_SEM;
            sops.sem_op = -1;
            semop(sem_id, &sops, 1);

            dump_navi[2]++; /* ++NAVE IN PORTO CARICO*/
            dump_navi[1]--; /* --NAVE IN MARE SCARICA*/
            dump_navi[7]++; /* ++NUM SCAMBI*/

            sops.sem_num = DUMP_NAVI_SEM;
            sops.sem_op = 1;
            semop(sem_id, &sops, 1);

            sops.sem_num = DUMP_PORTI_SEM;
            sops.sem_op = -1;
            semop(sem_id, &sops, 1);

            dump_porti[(comanda.id_porto_offerta * 7) + 6]++; /*banchine occupate*/

            sops.sem_num = DUMP_PORTI_SEM;
            sops.sem_op = 1;
            semop(sem_id, &sops, 1);

            /* controllo la merce prima di caricarla*/
            if (offerte[comanda.id_porto_offerta].scadenza <= 0) {

                /* semafori banchina: rilascio banchina*/
                sops.sem_num = NUM_SEMS + comanda.id_porto_offerta;
                sops.sem_op = 1;
                semop(sem_id, &sops, 1);

                sops.sem_num = DUMP_PORTI_SEM; /*rilascio banchina (--) dump*/
                sops.sem_op = -1;
                semop(sem_id, &sops, 1);

                dump_porti[(comanda.id_porto_offerta * 7) + 6]--;

                sops.sem_num = DUMP_PORTI_SEM;
                sops.sem_op = 1;
                semop(sem_id, &sops, 1);

                /* AGG. DUMP_MERCI */
                sops.sem_num = DUMP_MERCI_SEM;
                sops.sem_op = -1;
                semop(sem_id, &sops, 1);

                dump_merci[comanda.merce_type * 6 + 4] +=
                    comanda.merce_qta * registro_ptr[comanda.merce_type]; /* ++SCAD_PORTO */
                dump_merci[comanda.merce_type * 6 + 1] -=
                    comanda.merce_qta * registro_ptr[comanda.merce_type]; /* --QTA_PORTO */

                sops.sem_num = DUMP_MERCI_SEM;
                sops.sem_op = 1;
                semop(sem_id, &sops, 1);

                /* AGG. DUMP_PORTI*/
                sops.sem_num = DUMP_PORTI_SEM;
                sops.sem_op = -1;
                semop(sem_id, &sops, 1);

                dump_porti[comanda.id_porto_offerta * 7 + 2] -=
                    comanda.merce_qta * registro_ptr[comanda.merce_type]; /*        --QTA_PRESENTE*/

                sops.sem_num = DUMP_PORTI_SEM;
                sops.sem_op = 1;
                semop(sem_id, &sops, 1);

                sops.sem_num = LIST_NAVI_SEM;
                sops.sem_op = -1;
                semop(sem_id, &sops, 1);

                arrNave[id].stato = IN_VIAGGIO;

                sops.sem_num = LIST_NAVI_SEM;
                sops.sem_op = 1;
                semop(sem_id, &sops, 1);

                sops.sem_num = DUMP_NAVI_SEM;
                sops.sem_op = -1;
                semop(sem_id, &sops, 1);

                dump_navi[1]++; /*              ++NAVE MARE SCARICA */
                dump_navi[2]--; /*              --NAVE PORTO CARICO*/

                sops.sem_num = DUMP_NAVI_SEM;
                sops.sem_op = 1;
                semop(sem_id, &sops, 1);

                sops.sem_num = ANNUNCI_SEM;
                sops.sem_op = -1;
                sops.sem_flg = 0;
                semop(sem_id, &sops, 1);

                domande[comanda.d_index].qta += comanda.merce_qta;
                offerte[comanda.o_index].qta += 0;

                sops.sem_num = ANNUNCI_SEM;
                sops.sem_op = 1;
                sops.sem_flg = 0;
                semop(sem_id, &sops, 1);

            } else {
                /* la merce è buona quindi la carico*/
                carico = comanda.merce_qta * registro_ptr[comanda.merce_type];

                /* tempo x carico merce */
                sleep_carico_scarico(carico, vel_caricamento);

                messaggio.message_type = comanda.id_porto_offerta + 1;
                messaggio.payload[0] = comanda.merce_type;
                messaggio.payload[1] = -(carico);

                /* avviso il porto che devo caricare questa merce*/
                msgsnd(msg_id, &messaggio, sizeof(Message) - sizeof(long), 0);
                TEST_ERROR;

                sops.sem_num = DUMP_MERCI_SEM;
                sops.sem_op = -1;
                semop(sem_id, &sops, 1);

                dump_merci[comanda.merce_type * 6 + 2] += carico; /* ++QTA_NAVE*/
                dump_merci[comanda.merce_type * 6 + 1] -= carico; /* --QTA_PORTO*/

                sops.sem_num = DUMP_MERCI_SEM;
                sops.sem_op = 1;
                semop(sem_id, &sops, 1);

                /* semafori banchina: rilascio banchina*/
                sops.sem_num = NUM_SEMS + comanda.id_porto_offerta;
                sops.sem_op = 1;
                semop(sem_id, &sops, 1);

                sops.sem_num = DUMP_PORTI_SEM; /*rilascio banchina (--) dump*/
                sops.sem_op = -1;
                semop(sem_id, &sops, 1);

                dump_porti[(comanda.id_porto_offerta * 7) + 6]--;

                sops.sem_num = DUMP_PORTI_SEM;
                sops.sem_op = 1;
                semop(sem_id, &sops, 1);

                sops.sem_num = DUMP_NAVI_SEM;
                sops.sem_op = -1;
                semop(sem_id, &sops, 1);

                dump_navi[0]++; /* ++NAVE MARE CARICO */
                dump_navi[2]--; /* --NAVE PORTO CARICO */

                sops.sem_num = DUMP_NAVI_SEM;
                sops.sem_op = 1;
                semop(sem_id, &sops, 1);

                sops.sem_num = LIST_NAVI_SEM;
                sops.sem_op = -1;
                semop(sem_id, &sops, 1);

                arrNave[id].stato = IN_VIAGGIO;

                sops.sem_num = LIST_NAVI_SEM;
                sops.sem_op = 1;
                semop(sem_id, &sops, 1);

                /* viaggio verso il porto della domanda dalla posizione corrente*/
                sleep_percorso(current_pos, porto_D, vel_caricamento);
                current_pos = porto_D;

                /* controllo sulla scadenza merce */
                if (offerte[comanda.id_porto_offerta].scadenza <= 0) {

                    /* AGG. DUMP_MERCI */
                    sops.sem_num = DUMP_MERCI_SEM;
                    sops.sem_op = -1;
                    semop(sem_id, &sops, 1);

                    dump_merci[comanda.merce_type * 6 + 5] += carico; /* ++SCAD_NAVE*/
                    dump_merci[comanda.merce_type * 6 + 2] -= carico; /* --QTA_NAVE*/

                    sops.sem_num = DUMP_MERCI_SEM;
                    sops.sem_op = 1;
                    semop(sem_id, &sops, 1);

                    sops.sem_num = DUMP_NAVI_SEM;
                    sops.sem_op = -1;
                    semop(sem_id, &sops, 1);

                    dump_navi[1]++; /*              ++NAVE MARE SCARICA */
                    dump_navi[0]--; /*              --NAVE MARE CARICO*/

                    sops.sem_num = DUMP_NAVI_SEM;
                    sops.sem_op = 1;
                    semop(sem_id, &sops, 1);

                    sops.sem_num = ANNUNCI_SEM;
                    sops.sem_op = -1;
                    sops.sem_flg = 0;
                    semop(sem_id, &sops, 1);

                    domande[comanda.d_index].qta += comanda.merce_qta;
                    offerte[comanda.o_index].qta += 0;

                    sops.sem_num = ANNUNCI_SEM;
                    sops.sem_op = 1;
                    sops.sem_flg = 0;
                    semop(sem_id, &sops, 1);
                } else {
                    /*merce ancora buona, procedo a scaricarla*/

                    sops.sem_num = LIST_NAVI_SEM;
                    sops.sem_op = -1;
                    semop(sem_id, &sops, 1);

                    arrNave[id].stato = IN_PORTO;

                    sops.sem_num = LIST_NAVI_SEM;
                    sops.sem_op = 1;
                    semop(sem_id, &sops, 1);

                    sops.sem_num = NUM_SEMS + comanda.id_porto_domanda;
                    sops.sem_op = -1;
                    semop(sem_id, &sops, 1);

                    sops.sem_num = DUMP_PORTI_SEM; /*attracco banchina (++) dump*/
                    sops.sem_op = -1;
                    semop(sem_id, &sops, 1);

                    dump_porti[(comanda.id_porto_domanda * 7) + 6]++;

                    sops.sem_num = DUMP_PORTI_SEM;
                    sops.sem_op = 1;
                    semop(sem_id, &sops, 1);

                    sops.sem_num = DUMP_NAVI_SEM;
                    sops.sem_op = -1;
                    semop(sem_id, &sops, 1);

                    dump_navi[3]++; /*              ++NAVE PORTO SCARICO*/
                    dump_navi[0]--; /*              --NAVE MARE CARICO  */

                    sops.sem_num = DUMP_NAVI_SEM;
                    sops.sem_op = 1;
                    semop(sem_id, &sops, 1);

                    /*tempo x scarico merce*/
                    sleep_carico_scarico(carico, vel_caricamento);

                    messaggio.message_type = comanda.id_porto_domanda + 1;
                    messaggio.payload[1] = carico;

                    msgsnd(msg_id, &messaggio, sizeof(Message) - sizeof(long), 0);
                    TEST_ERROR;

                    sops.sem_num = DUMP_MERCI_SEM;
                    sops.sem_op = -1;
                    semop(sem_id, &sops, 1);

                    dump_merci[comanda.merce_type * 6 + 1] += carico; /*          ++QTA_PORTO*/
                    dump_merci[comanda.merce_type * 6 + 2] -= carico; /*          --QTA_NAVE*/
                    dump_merci[comanda.merce_type * 6 + 3] += carico; /*          ++QTA_CONSEGNATA*/

                    sops.sem_num = DUMP_MERCI_SEM;
                    sops.sem_op = 1;
                    semop(sem_id, &sops, 1);

                    /* rilascio banchina*/
                    sops.sem_num = NUM_SEMS + comanda.id_porto_domanda;
                    sops.sem_op = 1;
                    semop(sem_id, &sops, 1);

                    sops.sem_num = DUMP_PORTI_SEM; /*rilascio banchina (--) dump*/
                    sops.sem_op = -1;
                    semop(sem_id, &sops, 1);

                    dump_porti[(comanda.id_porto_domanda * 7) + 6]--;

                    sops.sem_num = DUMP_PORTI_SEM;
                    sops.sem_op = 1;
                    semop(sem_id, &sops, 1);

                    sops.sem_num = DUMP_NAVI_SEM;
                    sops.sem_op = -1;
                    semop(sem_id, &sops, 1);

                    dump_navi[1]++; /*          ++NAVE MARE SCARICA*/
                    dump_navi[3]--; /*          --NAVE PORTO SCARICO*/
                    dump_navi[6]++; /*          ++SCAMBIO ANDATO A BUON FINE*/

                    sops.sem_num = DUMP_NAVI_SEM;
                    sops.sem_op = 1;
                    semop(sem_id, &sops, 1);

                    sops.sem_num = LIST_NAVI_SEM;
                    sops.sem_op = -1;
                    semop(sem_id, &sops, 1);

                    arrNave[id].stato = IN_VIAGGIO;

                    sops.sem_num = LIST_NAVI_SEM;
                    sops.sem_op = 1;
                    semop(sem_id, &sops, 1);

                    carico = 0;
                }
            }
        } else {
            printf("Nave %d: id: %d I'll wait one sec \n", id, getpid());
            sleep(1);
            continue;
        }
    }
    printf("NAVE| %d | %d |: HO FINITO!\n", id, getpid());
    signal_handler(SIGINT);
}
