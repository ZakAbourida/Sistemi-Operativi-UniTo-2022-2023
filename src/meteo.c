#include "../lib/lib_so.h"

key_t sem_id, dump_porti_key, dump_meteo_key, config_key, shm_navi_k, shm_porto_k;
int *dump_porti, *dump_meteo, timer_maelstorm, prec;
pid_t *porti_pids, mareggiata;
Conf_values *config_ptr;
struct sembuf sops;
Nave *nave;
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
    TEST_ERROR;
}

void signal_handler(int sig) {
    if (sig == SIGUSR1)
        (void)write(2, "\n::METEO:: SIGUSR1 ricevuto\n", 29);
    else if (sig == SIGUSR2)
        (void)write(2, "\n::METEO:: SIGUSR2 ricevuto\n", 29);

    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    TEST_ERROR;
    exit(EXIT_SUCCESS);
}

void Tempesta() {
    Nave tmp;
    do {
        tmp = nave[rand() % config_ptr->SO_NAVI];
    } while (tmp.stato == IN_PORTO);
    kill(tmp.pid, SIGUSR1);
}

void Mareggiata() {
    int i;
    do {                                   /* serve per evitare che un porto sia colpito due volte di seguito*/
        i = rand() % config_ptr->SO_PORTI; /* prende un porto a caso*/
        mareggiata = porti_pids[i];
    } while (prec == i);
    prec = i;
    fprintf(stderr, "METEO | MAREGGIATA --> PORTO: %d \n", i);
    if (kill(mareggiata, SIGUSR1) == -1)
        TEST_ERROR;

    dump_meteo[i] = 1;
}

void Maelstorm() {
    Nave tmp;
    do {
        tmp = nave[rand() % config_ptr->SO_NAVI];
    } while (tmp.stato == IN_PORTO);
    kill(tmp.pid, SIGUSR2);
    TEST_ERROR;
}

int main(int argc, char const *argv[]) {
    struct timespec time_s, ORA;
    struct sigaction saMeteo;
    int i, timer, orePassate;

    BZERO(&saMeteo, sizeof(saMeteo));
    saMeteo.sa_handler = signal_handler;
    sigfillset(&mask); /* creo una maschera con tutti i segnali all'interno */

    sigdelset(&mask, SIGINT); /* vengono tolti dalla maschera i seguenti segnali*/
    sigdelset(&mask, SIGTERM);
    sigdelset(&mask, SIGALRM);

    sigaction(SIGINT, &saMeteo, NULL);
    sigaction(SIGTERM, &saMeteo, NULL);
    sigaction(SIGUSR1, &saMeteo, NULL);
    sigaction(SIGUSR2, &saMeteo, NULL);

    sigprocmask(SIG_SETMASK, &mask, NULL); /* maschero tutti i segnali tranne SIGUSR1 e SIGUSR2 così evito problemi*/
    TEST_ERROR;
    sigprocmask(SIG_BLOCK, &mask, NULL);
    TEST_ERROR;

    sem_id = atoi(argv[1]);
    dump_porti_key = atoi(argv[2]);
    config_key = atoi(argv[3]);
    shm_navi_k = atoi(argv[4]);
    shm_porto_k = atoi(argv[5]);
    dump_meteo_key = atoi(argv[6]);

    srand(time(NULL) ^ getpid());

    /*SEGMENTI MEMORIA CONDIVISA*/
    config_ptr = (Conf_values *)shmat(config_key,
                                      NULL,
                                      SHM_RDONLY);
    TEST_ERROR;

    dump_porti = shmat(dump_porti_key, NULL, 0);
    TEST_ERROR;

    dump_meteo = shmat(dump_meteo_key, NULL, 0);
    TEST_ERROR;

    nave = shmat(shm_navi_k, NULL, 0);
    TEST_ERROR;

    porti_pids = shmat(shm_porto_k, NULL, 0);
    TEST_ERROR;

    timer = config_ptr->SO_DAYS;
    timer_maelstorm = config_ptr->SO_MALESTORM * H_IN_NSEC; /* serve per la conversione corretta (3600 = 3600 sec / 1 ora) */

    time_s.tv_nsec = 1;
    time_s.tv_sec = 1;

    fprintf(stderr, "Meteo partito!\n");

    sops.sem_num = VIA_SEM;
    sops.sem_op = -1;
    semop(sem_id, &sops, 1);

    orePassate = 0;
    ORA.tv_nsec = H_IN_NSEC;
    ORA.tv_sec = 0;

    prec = -1;

    while (1) {
        safeWait(ORA.tv_sec, ORA.tv_nsec);
        TEST_ERROR;
        ORA.tv_nsec = H_IN_NSEC;
        ORA.tv_sec = 0;

        if (orePassate % 24 == 0) {
            Tempesta();
            TEST_ERROR;
            Mareggiata();
            TEST_ERROR;
        }

        if (orePassate % config_ptr->SO_MALESTORM == 0 && orePassate != 0) {
            Maelstorm();
            TEST_ERROR;
        }
        orePassate++;
    }

    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    TEST_ERROR;
    exit(EXIT_SUCCESS);
}
