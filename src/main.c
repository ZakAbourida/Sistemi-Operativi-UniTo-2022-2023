#include "../lib/lib_so.h"

#define MAX_LENGTH 100

key_t s_id, msg_id, dump_navi_key, dump_porti_key, dump_merci_key, config_key,
    domanda_key, offerta_key, registro_key, navi_shm_k, porti_shm_k, meteo_key;
pid_t child_pid, *porti_pids, meteo_pid, *meteo_dump;
Conf_values config, *config_ptr;
int i, j, *dump_navi, *registro, *dump_merci, *dump_porti, meta_porti, s;
Domanda *domande;
Offerta *offerte;
union semun su;
struct sembuf sops;
Nave *arrNavi;

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

void Dump_FINALE(int *NAVI, int *PORTI, int *MERCI) {
    int l;
    printf("\n|\t\tDUMP FINALE\t\t|\n\n");
    printf("| - NAVI IN MARE CON CARICO: %d\n", NAVI[0]);
    printf("| - NAVI IN MARE SENZA CARICO: %d\n", NAVI[1]);
    printf("| - NAVI CHE OCCUPANO BANCHINE: %d\n", NAVI[2] + NAVI[3]);
    printf("|\n");
    for (l = 0; l < config.SO_MERCI; l++) {
        printf("| - MERCE %d\n", MERCI[l * 6 + 0]);
        printf("| \t\t- disponibile: %d\n", MERCI[l * 6 + 1]);
        printf("| \t\t- in nave: %d\n", MERCI[l * 6 + 2]);
        printf("| \t\t- consegnata: %d\n", MERCI[l * 6 + 3]);
        printf("| \t\t- scaduta in porto: %d\n", MERCI[l * 6 + 4]);
        printf("| \t\t- scaduta in nave: %d\n", MERCI[l * 6 + 5]);
    }
    printf("|\n");
    for (l = 0; l < config.SO_PORTI; l++) {
        printf("| - PORTO %d\n", l);
        printf("| \t\t- presente: %d\n", PORTI[l * 7 + 2]);
        printf("| \t\t- spedita: %d\n", PORTI[l * 7 + 3]);
        printf("| \t\t- ricevuta: %d\n", PORTI[l * 7 + 4]);
    }
    printf("\n");
    for (l = 0; l < config.SO_MERCI; l++) {
        printf("| - MERCE %d\n", MERCI[l * 6 + 0]);
        printf("| - IL PORTO CHE HA OFFERTO DI PIU': PORTO %d\n",
               maxConsPorto(l));
        printf("| - IL PORTO CHE HA RICHIESTO DI PIU': PORTO %d\n",
               maxRicPorto(l));
    }
    printf("\n");
    printf("| - NAVI RALLENTATE DA TEMPESTE: %d\n", NAVI[4]);
    printf("| - NAVI AFFONDATE DA MAELSTORM: %d\n", NAVI[5]);
    printf("| - PORTI COLPITI DA MAREGGIATE: ");
    for (l = 0; l < config.SO_PORTI; l++) {
        if (meteo_dump[l])
            printf(" %d -", l);
    }
    printf("\n");
    printf("Scambi totali compiuti: %d\n", NAVI[6]);
    printf("Scambi totali incominciati: %d\n", NAVI[7]);
}

void Stampa_Dump(int *dump_navi_ptr, int *dump_porti_ptr, int *dump_merci_ptr, Conf_values *Config) {

    /** TODO: AGGIUNGERE SEMAFORO E FARE NAVI - NAVI_AFFONDATE */
    Config->SO_NAVI = config.SO_NAVI;
    Config->SO_NAVI = Config->SO_NAVI - dump_navi_ptr[5];

    printf("\n| Navi: %d |\n| Porti: %d |\n| Merci: %d |\n", Config->SO_NAVI,
           config.SO_PORTI, config.SO_MERCI);

    printf("\n| NAVI IN MARE CON CARICO:\t%1d |\n| NAVI IN MARE SENZA CARICO:\t%1d |\n| NAVI IN PORTO_CARICO:\t\t%1d |\n| NAVI IN PORTO_SCARICO:\t%1d |\n",
           dump_navi_ptr[0], dump_navi_ptr[1], dump_navi_ptr[2], dump_navi_ptr[3]);

    printf("\n\033[1m%12s\t \t%12s\t \t%12s\t\t %12s\t\t %12s\t\t %12s\t\t %12s\t\033[0m", "PORTO (x)", "PORTO (y)", "QTA_PRESENTE",
           "QTA_SPEDITA", "QTA_RICEVUTA", "BANCHINE_TOT", "BANCHINE_OCC");
    for (i = 0; i < config.SO_PORTI; i++) {
        printf("\n");
        for (j = 0; j < 7; j++) {
            printf("\033[1m%8d\t\t\033[0m", dump_porti_ptr[i * 7 + j]);
        }
    }

    printf("\n\033[1m%12s\t %12s\t %12s\t %12s\t %12s\t %12s \033[0m", "MERCE", "QTA_PORTO",
           "QTA_NAVE", "QTA_CONS", "SCAD_PORTO", "SCAD_NAVE");
    for (i = 0; i < config.SO_MERCI; i++) {
        printf("\n");
        for (j = 0; j < 6; j++) {
            printf("\033[1m%12d\t\033[0m", dump_merci_ptr[i * 6 + j]);
        }
    }
}

void signal_handler(int sig) {
    int status;

    if (sig == SIGALRM) { /*Chiamata di sistema write():    | stderr(2) | stringa | lunghezza stringa |*/
        (void)!write(2, "\n::MASTER:: TIME IS UP \n", 25);
        killpg(0, SIGTERM);
    } else if (sig == SIGINT) {
        (void)!write(2, "\n::MASTER:: SIGINT RICEVUTO\n", 29);
        killpg(0, SIGTERM);

        /*distruzione semafori e coda di messaggi*/
        semctl(s_id, 0, IPC_RMID);      /*semafori*/
        msgctl(msg_id, IPC_RMID, NULL); /*coda di messaggi*/

        while ((child_pid = wait(&status)) != -1) {
            /* aspettiamo che tutti i processi terminino*/
        }
        Dump_FINALE(dump_navi, dump_porti, dump_merci);

        exit(EXIT_SUCCESS);
    } else if (sig == SIGCHLD) {
        if (dump_navi[5] == config.SO_NAVI) {
            fprintf(stderr, "::MASTER:: Non ci sono più navi in circolazione \n");
            killpg(0, SIGTERM);
        } else
            return;
    } else if (sig == SIGTERM) {
        /*distruzione semafori e coda di messaggi*/
        semctl(s_id, 0, IPC_RMID);      /*semafori*/
        msgctl(msg_id, IPC_RMID, NULL); /*coda di messaggi*/

        while ((child_pid = wait(&status)) != -1) {
            /* aspettiamo che tutti i processi terminino*/
        }

        Dump_FINALE(dump_navi, dump_porti, dump_merci);
        exit(EXIT_SUCCESS);
    }
}

int maxConsPorto(int merce) {
    int max, p, q, s, *arrayPorti;
    max = 0;

    arrayPorti = (int *)calloc((config.SO_PORTI / 2), sizeof(int));

    for (s = 0; s < config.SO_PORTI / 2; s++) {
        for (q = 0;
             q < config.SO_MERCI * (config.SO_PORTI / 2) * config.SO_DAYS;
             q++) {
            if (offerte[q].merce_type == merce && offerte[q].porto == s) {
                arrayPorti[s] += offerte[q].qta;
            }
        }
    }
    p = 0;
    max = arrayPorti[0];

    for (q = 0; q < config.SO_PORTI / 2; q++) {
        if (arrayPorti[q] < max) {
            max = arrayPorti[q];
            p = q;
        }
    }
    free(arrayPorti);
    return offerte[p].porto;
}

int maxRicPorto(int merce) {
    int max, mMax, p, q;
    max = 0;
    p = 0;
    mMax = config.SO_FILL / (config.SO_PORTI / 2 + config.SO_PORTI % 2);

    for (q = 0;
         q < config.SO_MERCI * (config.SO_PORTI / 2 + config.SO_PORTI % 2);
         q++) {
        if (domande[q].merce_type == merce && (mMax - domande[q].qta) > max) {
            max = mMax - domande[q].qta;
            p = q;
        }
    }
    return domande[p].porto;
}

/* metodo per lettura da file di configurazione fgets salva nella stringa anche \n quindi lo cambio con \0 */
void getch(FILE *fp, char stringa[MAX_LENGTH]) {
    int len;
    fgets(stringa, MAX_LENGTH, fp);
    len = strlen(stringa);
    if (len > 0 && stringa[len - 1] == '\n') {
        stringa[len - 1] = '\0';
    }
}

void generate_register() {
    int z;
    for (z = 0; z < config.SO_MERCI; z++) {
        registro[z] = (rand() % (config.SO_SIZE)) + 1;
    }
}

/*metodo generazione Offerta*/
void generate_O(int timer) {
    int giorni_passati, i, j;

    if (timer == 0) {
        return;
    }

    giorni_passati = config.SO_DAYS - timer;

    j = -1;
    for (i = 0; i < (config.SO_PORTI / 2) * config.SO_MERCI; i++) {
        if (i % config.SO_MERCI == 0) {
            j++;
        }
        offerte[giorni_passati * (config.SO_PORTI / 2) * config.SO_MERCI + i]
            .porto = j;
        offerte[giorni_passati * (config.SO_PORTI / 2) * config.SO_MERCI + i]
            .merce_type = i % config.SO_MERCI;
        offerte[giorni_passati * (config.SO_PORTI / 2) * config.SO_MERCI + i]
            .qta = ((config.SO_FILL / (config.SO_PORTI / 2)) / config.SO_DAYS) / registro[i % config.SO_MERCI]; /*in lotti*/
        offerte[giorni_passati * (config.SO_PORTI / 2) * config.SO_MERCI + i]
            .scadenza = rand() % (config.SO_MAX_VITA - config.SO_MIN_VITA + 1) + config.SO_MIN_VITA;
    }
    sops.sem_num = DUMP_PORTI_SEM;
    sops.sem_op = -1;
    semop(s_id, &sops, 1);

    for (i = 0; i < config.SO_PORTI / 2; i++) {
        dump_porti[i * 7 + 2] +=
            config.SO_MERCI * ((config.SO_FILL / (config.SO_PORTI / 2)) / config.SO_DAYS);
    }

    sops.sem_num = DUMP_PORTI_SEM;
    sops.sem_op = 1;
    semop(s_id, &sops, 1);

    sops.sem_num = DUMP_MERCI_SEM;
    sops.sem_op = -1;
    semop(s_id, &sops, 1);

    for (i = 0; i < config.SO_MERCI; i++) {
        dump_merci[i * 6 + 1] += config.SO_FILL / config.SO_DAYS;
    }

    sops.sem_num = DUMP_MERCI_SEM;
    sops.sem_op = 1;
    semop(s_id, &sops, 1);
}

void invecchiamentoMerci() {
    int f;
    for (f = 0;
         f < config.SO_MERCI * (config.SO_PORTI / 2) * config.SO_DAYS;
         f++) {
        offerte[f].scadenza--;
    }
}

void load_conf(Conf_values *config_ptr, char const *argv[]) {
    FILE *fp;
    char stringa[MAX_LENGTH];
    fp = fopen("conf.txt", "r");
    if (fp == NULL) {
        fprintf(stderr, "Impossibile aprire il file di configurazione\n");
        exit(EXIT_FAILURE);
    }

    do {
        getch(fp, stringa);
    } while (strcmp(stringa, argv[1]) != 0 && !feof(fp));

    if (feof(fp)) {
        fprintf(stderr,
                "Problemi nella lettura del file: configurazione non trovata\n");
        exit(EXIT_FAILURE);
    }

    getch(fp, stringa);
    config_ptr->SO_NAVI = atoi(stringa);
    config.SO_NAVI = atoi(stringa);
    getch(fp, stringa);
    config_ptr->SO_PORTI = atoi(stringa);
    config.SO_PORTI = atoi(stringa);
    getch(fp, stringa);
    config_ptr->SO_MERCI = atoi(stringa);
    config.SO_MERCI = atoi(stringa);
    getch(fp, stringa);
    config_ptr->SO_SIZE = atoi(stringa);
    config.SO_SIZE = atoi(stringa);
    getch(fp, stringa);
    config_ptr->SO_MIN_VITA = atoi(stringa);
    config.SO_MIN_VITA = atoi(stringa);
    getch(fp, stringa);
    config_ptr->SO_MAX_VITA = atoi(stringa);
    config.SO_MAX_VITA = atoi(stringa);
    getch(fp, stringa);
    config_ptr->SO_LATO = atoi(stringa);
    config.SO_LATO = atoi(stringa);
    getch(fp, stringa);
    config_ptr->SO_SPEED = atoi(stringa);
    config.SO_SPEED = atoi(stringa);
    getch(fp, stringa);
    config_ptr->SO_CAPACITY = atoi(stringa);
    config.SO_CAPACITY = atoi(stringa);
    getch(fp, stringa);
    config_ptr->SO_BANCHINE = atoi(stringa);
    config.SO_BANCHINE = atoi(stringa);
    getch(fp, stringa);
    config_ptr->SO_FILL = atoi(stringa);
    config.SO_FILL = atoi(stringa);
    getch(fp, stringa);
    config_ptr->SO_LOADSPEED = atoi(stringa);
    config.SO_LOADSPEED = atoi(stringa);
    getch(fp, stringa);
    config_ptr->SO_DAYS = atoi(stringa);
    config.SO_DAYS = atoi(stringa);
    getch(fp, stringa);
    config_ptr->SO_STORM_DURATION = atoi(stringa);
    config.SO_STORM_DURATION = atoi(stringa);
    getch(fp, stringa);
    config_ptr->SO_SWELL_DURATION = atoi(stringa);
    config.SO_SWELL_DURATION = atoi(stringa);
    getch(fp, stringa);
    config_ptr->SO_MALESTORM = atoi(stringa);
    config.SO_MALESTORM = atoi(stringa);

    fclose(fp);
}

void initializeResources(char const *argv[]) {
    /* -- MESSSAGGI -- */
    msg_id = msgget(IPC_PRIVATE, 0666);
    TEST_ERROR;

    /**
     * Creazione dei 3 segmenti della memoria condivisa:
     *  -Segmento 1: contiene il dump
     *  -Segmento 2: contiene la struct con i dati della configurazione
     *  -Segmento 3: contiene gli annunci
     */
    config_key = shmget(IPC_PRIVATE, sizeof(Conf_values), IPC_CREAT | 0666);
    TEST_ERROR;

    config_ptr = (Conf_values *)shmat(config_key, NULL, 0);
    TEST_ERROR;

    load_conf(config_ptr, argv);
    TEST_ERROR;

    if (config.SO_PORTI < 4) {
        fprintf(stderr, "Il numero di porti deve essere maggiore o uguale a 4\n");
        shmctl(config_key, IPC_RMID, NULL);
        TEST_ERROR;
        msgctl(msg_id, IPC_RMID, NULL); /*coda di messaggi*/
        TEST_ERROR;

        exit(EXIT_FAILURE);
    }

    dump_navi_key = shmget(IPC_PRIVATE,
                           sizeof(int) * 8,
                           IPC_CREAT | 0666); /* 4 stati della nave + tempesta + maelstrom*/
    TEST_ERROR;

    dump_porti_key = shmget(IPC_PRIVATE,
                            sizeof(int) * config.SO_PORTI * 7,
                            IPC_CREAT | 0666);
    TEST_ERROR;

    dump_merci_key = shmget(IPC_PRIVATE,
                            sizeof(int) * config.SO_MERCI * 6,
                            IPC_CREAT | 0666);
    TEST_ERROR;

    domanda_key = shmget(IPC_PRIVATE,
                         sizeof(Domanda) * config.SO_MERCI * (config.SO_PORTI / 2 + config.SO_PORTI % 2),
                         IPC_CREAT | 0666);
    TEST_ERROR;

    offerta_key = shmget(IPC_PRIVATE,
                         sizeof(Offerta) * config.SO_MERCI * (config.SO_PORTI / 2) * config.SO_DAYS,
                         IPC_CREAT | 0666);
    TEST_ERROR;

    registro_key = shmget(IPC_PRIVATE,
                          sizeof(int) * config.SO_MERCI,
                          IPC_CREAT | 0666);
    TEST_ERROR;

    navi_shm_k = shmget(IPC_PRIVATE,
                        izeof(Nave) * config.SO_NAVI,
                        IPC_CREAT | 0666);
    TEST_ERROR;

    /* dump_meteo[i] = porto [i] mareggiato si o no */
    meteo_key = shmget(IPC_PRIVATE,
                       sizeof(int) * config.SO_PORTI,
                       IPC_CREAT | 0666);
    TEST_ERROR;

    porti_shm_k = shmget(IPC_PRIVATE,
                         sizeof(pid_t) * config.SO_PORTI,
                         IPC_CREAT | 0666);
    TEST_ERROR;

    /*ATTACHMENT*/
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

    porti_pids = shmat(porti_shm_k, NULL, 0);
    TEST_ERROR;

    arrNavi = shmat(navi_shm_k, NULL, 0);
    TEST_ERROR;

    meteo_dump = shmat(meteo_key, NULL, 0);
    TEST_ERROR;

    /*Marco i segmenti in modo tale che si elimino non appena viene terminato il processo oppure viene fatto il detatch*/
    shmctl(dump_navi_key, IPC_RMID, NULL);
    TEST_ERROR;
    shmctl(dump_porti_key, IPC_RMID, NULL);
    TEST_ERROR;
    shmctl(dump_merci_key, IPC_RMID, NULL);
    TEST_ERROR;
    shmctl(config_key, IPC_RMID, NULL);
    TEST_ERROR;
    shmctl(domanda_key, IPC_RMID, NULL);
    TEST_ERROR;
    shmctl(offerta_key, IPC_RMID, NULL);
    TEST_ERROR;
    shmctl(registro_key, IPC_RMID, NULL);
    TEST_ERROR;
    shmctl(navi_shm_k, IPC_RMID, NULL);
    TEST_ERROR;
    shmctl(porti_shm_k, IPC_RMID, NULL);
    TEST_ERROR;
    shmctl(meteo_key, IPC_RMID, NULL);
    TEST_ERROR;

    s_id = semget(IPC_PRIVATE, NUM_SEMS + config_ptr->SO_PORTI, 0666);
    TEST_ERROR;

    su.val = 0;
    for (i = 0; i < NUM_SEMS + config_ptr->SO_PORTI; i++) {
        semctl(s_id, i, SETVAL, su);
        TEST_ERROR;
    }
}

void initializeData() {
    Offerta tmp;
    int porto, merce, i, j;

    /** DUMP NAVI*/
    dump_navi[0] = 0;              /* numero di navi in mare con carico */
    dump_navi[1] = config.SO_NAVI; /* numero di navi in mare senza carico */
    dump_navi[2] = 0;              /* numero di navi in porto con carico */
    dump_navi[3] = 0;              /* numero di navi in porto senza carico */
    dump_navi[4] = 0;              /* numero di navi rallentate dalle tempeste */
    dump_navi[5] = 0;              /* numero di navi affondate da maelstorm */
    dump_navi[6] = 0;              /* numero scambi andati a buon fine*/
    dump_navi[7] = 0;              /* numero scambi iniziati*/

    /** DUMP MERCI*/
    for (i = 0; i < config.SO_MERCI; i++) {
        dump_merci[i * 6 + 0] = i; /* id_merce*/
        dump_merci[i * 6 + 1] = 0; /* qta_porto*/
        dump_merci[i * 6 + 2] = 0; /* qta_nave*/
        dump_merci[i * 6 + 3] = 0; /* qta_cons*/
        dump_merci[i * 6 + 4] = 0; /* scad_porto */
        dump_merci[i * 6 + 5] = 0; /* scad_nave */
    }

    /** DOMANDE*/
    porto = config.SO_PORTI / 2;
    merce = 0;

    j = 0;
    for (i = 0; i < (config.SO_PORTI / 2 + config.SO_PORTI % 2) * config.SO_MERCI; i++) {
        if (i % (config.SO_PORTI / 2 + config.SO_PORTI % 2) == 0 && i != 0) {
            j++;
        }
        domande[i].merce_type = j;
        domande[i].porto =
            i % (config.SO_PORTI / 2 + config.SO_PORTI % 2) + config.SO_PORTI / 2;
        domande[i].qta =
            (config.SO_FILL / (config.SO_PORTI / 2 + config.SO_PORTI % 2)) / registro[j];
    }

    /* inizializzo il dump meteo*/
    for (i = 0; i < config.SO_PORTI; i++) {
        meteo_dump[i] = 0;
    }
}

int main(int argc, char const *argv[]) {
    /*Declaration of variables*/
    int status, tmp;
    struct sigaction sa;
    char *args_porti[12] = {"porto"};
    char *args_navi[13] = {"nave"};
    char *args_meteo[8] = {"meteo"};
    char s_id_str[3 * sizeof(s_id) + 1];
    char msg_id_str[3 * sizeof(msg_id) + 1];
    char dump_navi_id_str[3 * sizeof(dump_navi_key) + 1];
    char dump_porti_id_str[3 * sizeof(dump_porti_key) + 1];
    char dump_merci_id_str[3 * sizeof(dump_merci_key) + 1];
    char shm_config_str[3 * sizeof(config_key) + 1];
    char shm_domanda_str[3 * sizeof(domanda_key) + 1];
    char shm_offerta_str[3 * sizeof(offerta_key) + 1];
    char shm_registro_str[3 * sizeof(registro_key) + 1];
    char shm_navi_str[3 * sizeof(navi_shm_k) + 1];
    char shm_porti_str[3 * sizeof(porti_shm_k) + 1];
    char shm_meteo_str[3 * sizeof(meteo_key) + 1];
    char id_str[4];
    unsigned int timer;

    srand(time(NULL));

    /** -- SIGNAL HANDLER --
     * bzero(): - prima setta tutti i bit di sa a 0
     *          - poi inizializza sa.handler a un puntatore alla funzione
     * signal_handler
     *          - infine setta l'handler per gestire i segnali ((struct sigaction *oldact) = NULL))
     */
    BZERO(&sa, sizeof(sa));
    sa.sa_handler = signal_handler;

    sigaction(SIGALRM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGCHLD, &sa, NULL);

    /*inizializzazione risorse e dati*/
    initializeResources(argv);

    generate_register();

    initializeData();

    /* Semaphores set for Annunci*/
    su.val = 1;
    semctl(s_id, ANNUNCI_SEM, SETVAL, su);

    sops.sem_num = VIA_SEM;
    sops.sem_op = 0;
    sops.sem_flg = 0;
    semop(s_id, &sops, 1);

    sops.sem_num = DUMP_PORTI_SEM;
    sops.sem_op = 1;
    semop(s_id, &sops, 1);

    sops.sem_num = DUMP_MERCI_SEM;
    sops.sem_op = 1;
    semop(s_id, &sops, 1);

    sops.sem_num = DUMP_NAVI_SEM;
    sops.sem_op = 1;
    semop(s_id, &sops, 1);

    sops.sem_num = LIST_NAVI_SEM;
    sops.sem_op = 1;
    semop(s_id, &sops, 1);

    /* Preparing command-line arguments for childs' execve */
    sprintf(s_id_str, "%d", s_id);
    sprintf(msg_id_str, "%d", msg_id);
    sprintf(dump_navi_id_str, "%d", dump_navi_key);
    sprintf(dump_porti_id_str, "%d", dump_porti_key);
    sprintf(dump_merci_id_str, "%d", dump_merci_key);
    sprintf(shm_config_str, "%d", config_key);
    sprintf(shm_domanda_str, "%d", domanda_key);
    sprintf(shm_offerta_str, "%d", offerta_key);
    sprintf(shm_registro_str, "%d", registro_key);
    sprintf(shm_navi_str, "%d", navi_shm_k);
    sprintf(shm_porti_str, "%d", porti_shm_k);
    sprintf(shm_meteo_str, "%d", meteo_key);

    args_navi[1] = s_id_str;
    args_navi[2] = msg_id_str;
    args_navi[3] = dump_navi_id_str;
    args_navi[4] = dump_porti_id_str;
    args_navi[5] = dump_merci_id_str;
    args_navi[6] = shm_config_str;
    args_navi[7] = shm_domanda_str;
    args_navi[8] = shm_offerta_str;
    args_navi[9] = shm_registro_str;
    args_navi[10] = shm_navi_str;

    args_porti[1] = s_id_str;
    args_porti[2] = msg_id_str;
    args_porti[3] = dump_navi_id_str;
    args_porti[4] = dump_porti_id_str;
    args_porti[5] = dump_merci_id_str;
    args_porti[6] = shm_config_str;
    args_porti[7] = shm_domanda_str;
    args_porti[8] = shm_offerta_str;
    args_porti[9] = shm_registro_str;

    args_meteo[1] = s_id_str;
    args_meteo[2] = dump_porti_id_str;
    args_meteo[3] = shm_config_str;
    args_meteo[4] = shm_navi_str;
    args_meteo[5] = shm_porti_str;
    args_meteo[6] = shm_meteo_str;
    args_meteo[7] = NULL;

    printf("my pid:\t%d \n\n", getpid());

    /* inizializza il timer e genera il primo set di offerte */
    timer = config.SO_DAYS;
    generate_O(timer);

    /*SPAWN FIGLI PORTI*/
    for (i = 0; i < config.SO_PORTI; i++) {
        sprintf(id_str, "%d", i);
        args_porti[10] = id_str;
        args_porti[11] = NULL;

        switch (fork()) {
        case -1:
            TEST_ERROR;
            break;
        case 0:
            porti_pids[i] = getpid();
            execve("./bin/porto", args_porti, NULL);
            TEST_ERROR;
            exit(EXIT_SUCCESS);
            break;
        default:
            break;
        }
    }

    /*SPAWN FIGLI PORTI*/
    for (i = 0; i < config.SO_NAVI; i++) {
        sprintf(id_str, "%d", i);
        args_navi[11] = id_str;
        args_navi[12] = NULL;

        switch (fork()) {
        case -1:
            TEST_ERROR;
            break;
        case 0:
            sops.sem_num = LIST_NAVI_SEM;
            sops.sem_op = -1;
            semop(s_id, &sops, 1);

            arrNavi[i].id = config_ptr->SO_PORTI + 1 + i;
            arrNavi[i].stato = IN_VIAGGIO;
            arrNavi[i].pid = getpid();

            sops.sem_num = LIST_NAVI_SEM;
            sops.sem_op = 1;

            semop(s_id, &sops, 1);
            execve("./bin/nave", args_navi, NULL);
            TEST_ERROR;
            exit(EXIT_SUCCESS);
            break;
        default:
            break;
        }
    }
    /*SPAWN FIGLO METEO*/
    switch (meteo_pid = fork()) {
    case -1:
        TEST_ERROR;
        break;
    case 0:
        execve("./bin/meteo", args_meteo, NULL);
        TEST_ERROR;
        break;
    default:
        break;
    }

    printf("PID:%d padre: figli pronti\n", getpid());

    /** SERVE PER STAMPARE TUTTE LE DOMANDE E LE OFFERTE PRIMA CHE I FIGLI PARTINO CON LE ROUTINE
    printf("\n\t\t\tOFFERTE\n");
    printf("|\tPorto\t|\tQta\t|\tTypeMerce\t|\tScadenza\t|\n");
    for (i = 0; i < (config_ptr->SO_MERCI * (config_ptr->SO_PORTI / 2) * config_ptr->SO_DAYS); i++) {
        printf("|\t%d\t|\t%d\t|\t%d\t|\t%d\t|\n", offerte[i].porto, offerte[i].qta, offerte[i].merce_type, offerte[i].scadenza);
    }
    printf("\n\t\t\tDOMANDE\n");
    printf("|\tPorto\t|\tQta\t|\tTypeMerce\t|\n");
    for (i = 0; i < config.SO_MERCI * (config.SO_PORTI / 2 + config.SO_PORTI % 2); i++) {
        printf("|\t%d\t|\t%d\t|\t%d\t|\n", domande[i].porto, domande[i].qta, domande[i].merce_type);
    }*/

    printf("PID:%d padre: 3\n", getpid());
    safeWait(1, 0);
    printf("PID:%d padre: 2\n", getpid());
    safeWait(1, 0);
    printf("PID:%d padre: 1\n", getpid());
    safeWait(1, 0);

    printf("PID:%d padre: VIA\n", getpid());

    sops.sem_num = VIA_SEM;
    sops.sem_op = (config.SO_NAVI + config.SO_PORTI) + 1;
    sops.sem_flg = 0;
    semop(s_id, &sops, 1);

    fprintf(stderr, "Lancio il timer (%d) e cronometro inizio simulazione\n",
            config.SO_DAYS);

    alarm(config.SO_DAYS);

    while (timer--) {
        printf("\n\n\t\t\t--\tDUMP\t--\n");
        printf("\n| Tempo Mancante:  %5d\033[34ms\033[0m |", timer);
        Stampa_Dump(dump_navi, dump_porti, dump_merci, config_ptr);
        invecchiamentoMerci();
        generate_O(timer);
        safeWait(1, 0);
    }
    signal_handler(SIGINT);
}
