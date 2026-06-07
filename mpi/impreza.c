/**
    Stowarzyszenie Upitych Artystów — wersja OpenMPI

  DO NAS KTÓRZY ZA CHWILĘ ZAPOMNĄ JAK TO KOMPILOWAĆ I URUCHAMIAĆ:
  Kompilacja:  mpicc -O2 -o impreza impreza.c
  Uruchomienie: mpirun --oversubscribe -np 10 -x CIRCLE_SIZE=4 ./impreza
 
 
*/

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <limits.h>

// stałe define 
#define MAX_CIRCLE_SIZE  16      
#define QUEUE_CAPACITY   64           



static int circle_size = 3; //nie patrzec na to 3, jest aby byla jakas wartosc inicjalizacji, i tak jest nadpisywana przez argument programu, wiec nie ma znaczenia, ale musi byc zainicjalizowana na jakas wartosc zeby program sie nie wyjebalil przy sprawdzaniu warunku czy jest organizatorem czy nie, bo to jest sprawdzane juz w mainie zanim circle_size zostanie nadpisane przez argument programu

#define PARTY_MS         5000    // czas trwania imprezy 
#define MIN_KAC_MS       5000    // min czas kacowania [ms]               
#define MAX_KAC_MS       15000   // max czas kacowania [ms]                
#define REQ_RETRY_MS     2000    // czas do retransmisji REQ [ms]          
#define REL_GRACE_MS     500     // pauza organizatora po wysłaniu REL     
#define TICK_MS          50      // czasy na tick, aby to też troszkę trwało          
#define MAX_PARTIES      1000       // ile imprez zanim proces kończy pracę   
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MS_TO_SEC(ms) ((ms) / 1000.0)


typedef enum {
    MSG_READY   = 0,  // bariera startowa 
    MSG_REQ     = 1,  // żądanie wejścia do kolejki                  
    MSG_ACK     = 2,  // potwierdzenie REQ                           
    MSG_REL     = 3,  // zwolnienie - koniec imprezy                 
    MSG_WELCOME = 4,  // zaproszenie do kółka (od organizatora)      
    MSG_HELLO   = 5,  // wymiana deficytów                           
} msg_type_t;

/

/* Historyczny bilans zasobów danego procesu */
typedef struct {
    uint8_t sep;        
    uint8_t alko;       
    uint8_t zagrycha;   
    uint8_t num_parties;
} deficit_t;

/*
    Płaska struktura wiadomości przesyłanej przez MPI.
    Wszystkie warianty używają tej samej - struktury sa nieużywane pola są zerowane.
 */
typedef struct {
    int       type;                          /* msg_type_t                   */
    int       from;                          /* rank nadawcy                 */
    uint64_t  ts;                            /* znacznik czasu Lamporta      */
    int       participants[MAX_CIRCLE_SIZE]; /* dla REL i WELCOME            */
    deficit_t deficit;                       /* dla WELCOME i HELLO          */
} msg_t;

/* Kolejka, to jest to co bylo w mqueue kiedys  */
typedef struct {
    int      rank;
    uint64_t ts;
} queue_entry_t;

typedef enum { SEPIA = 0, ALKOHOL = 1, ZAGRYCHA = 2 } contribution_t;

typedef enum {
    KACUJE,             // odpoczynek przed kolejną imprezą   
    WYSYLAM_REQ,        // wysyłam REQ i czekam na ACK od wszystkich
    JESTEM_W_KOLEJCE,   // czekam aż awansuję na pozycję k*CIRCLE_SIZE albo az kotoś rozpocznie impreze 
    UMAWIAM_IMPREZE,    // wymiana HELLO z uczestnikami       
    IMPREZA,            // sekcja krytyczna                   
} state_t;

typedef struct {
    int     rank;
    int     nprocs;
    state_t state;
    uint64_t lts;           // lokalny zegar Lamporta 

    // globalna kolejka 
    queue_entry_t queue[QUEUE_CAPACITY];
    int           queue_size;

    /* Faza REQ */
    bool     request_sent;
    bool     all_acks_received;
    uint64_t request_ts;    // ts z którym wysłano aktualny REQ       
    int      ack_count;
    uint64_t acked_mask;    // bit i = ACK od procesu i już odebrany   
    double   req_sent_at;   // to było do debugowania ale boje sie ze jak to dotknę to coś sie stanie wiec zostawiam 
    // Faza HELLO 
    bool hello_sent;
    int  hello_count;

    // Impreza 
    bool         is_organizer;
    int          participants[MAX_CIRCLE_SIZE];
    deficit_t    my_def;
    deficit_t    recv_def[MAX_CIRCLE_SIZE]; /* deficyty od uczestników   */
    contribution_t what_i_bring;
    bool         rel_received;          /* ustawiane przez on_message    */

    //bariera synchonizacyjna startowa, TAK WIEM NIE IALO BYC BARIER, ale startowa podobno mogła być, pozsotałosć z robienia tego w freeRTOSie 
    bool ready_to_kac;
    int  ready_count;

    // timery do odmierzania czasów 
    bool   kac_in_progress;
    double kac_until;       /* MPI_Wtime() kiedy skończyć kacowanie    */
    double party_until;     /* MPI_Wtime() kiedy skończyć imprezę      */

    // w zasadzie to też nie potrzebne juz, ale boje sie ze jak to dotkne to cos sie jebnie, a to bylo do debugowania, wiec zostawiam
    int parties_completed;
} proc_t;


//logiiiiii
static const char *state_name(state_t s) {
    switch (s) {
        case KACUJE:           return "KACUJE";
        case WYSYLAM_REQ:      return "WYSYLAM_REQ";
        case JESTEM_W_KOLEJCE: return "JESTEM_W_KOLEJCE";
        case UMAWIAM_IMPREZE:  return "UMAWIAM_IMPREZE";
        case IMPREZA:          return "IMPREZA";
    }
    return "?";
}

static void log_msg(const proc_t *p, const char *text) {
    printf("[P%02d][LT:%4llu] %s\n",
           p->rank, (unsigned long long)p->lts, text);
    fflush(stdout);
}

#define LOG(p, fmt, ...) \
    do { \
        char _buf[256]; \
        snprintf(_buf, sizeof(_buf), fmt, ##__VA_ARGS__); \
        log_msg(p, _buf); \
    } while (0)

static void set_state(proc_t *p, state_t new_state) {
    LOG(p, "%s -> %s", state_name(p->state), state_name(new_state));
    p->state = new_state;
}


// nowe funkcje do komunikacji MPI, zamiast ESP-NOW troszke przepis trosze nie 
static void send_to(int dest, const msg_t *m) {
    MPI_Send(m, sizeof(msg_t), MPI_BYTE, dest, 0, MPI_COMM_WORLD);
}

static void broadcast(const proc_t *p, const msg_t *m) {
    for (int i = 0; i < p->nprocs; i++)
        if (i != p->rank)
            MPI_Send(m, sizeof(msg_t), MPI_BYTE, i, 0, MPI_COMM_WORLD);
}

/* Zwraca true i wypełnia *m jeśli jest oczekująca wiadomość */
static bool poll_msg(msg_t *m) {
    MPI_Status status;
    int flag = 0;
    MPI_Iprobe(MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &flag, &status);
    if (!flag) return false;
    MPI_Recv(m, sizeof(msg_t), MPI_BYTE,
             status.MPI_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    return true;
}



//Kolejka Lamporta (posortowana wg (ts, rank))
static void mqueue_insert(proc_t *p, int rank, uint64_t ts) {
    /* dedup */
    for (int i = 0; i < p->queue_size; i++)
        if (p->queue[i].rank == rank) return;

    if (p->queue_size >= QUEUE_CAPACITY) return; /* na wszelki wypadek */

    int pos = p->queue_size;
    for (int i = 0; i < p->queue_size; i++) {
        if (ts < p->queue[i].ts ||
            (ts == p->queue[i].ts && rank < p->queue[i].rank)) {
            pos = i;
            break;
        }
    }

    for (int i = p->queue_size; i > pos; i--)
        p->queue[i] = p->queue[i - 1];

    p->queue[pos] = (queue_entry_t){ .rank = rank, .ts = ts };
    p->queue_size++;
}

static void mqueue_remove(proc_t *p, const int *ranks, int count) {
    for (int r = 0; r < count; r++) {
        for (int i = 0; i < p->queue_size; i++) {
            if (p->queue[i].rank == ranks[r]) {
                for (int j = i; j < p->queue_size - 1; j++)
                    p->queue[j] = p->queue[j + 1];
                p->queue_size--;
                break;
            }
        }
    }
}



// Podział zasobów (deterministyczny)
static void disposition(proc_t *p) {
    static const char *contrib_name[] = { "SEPIA", "ALKOHOL", "ZAGRYCHA" };
    bool           assigned[MAX_CIRCLE_SIZE] = { false };
    contribution_t result[MAX_CIRCLE_SIZE]   = { 0 };

/// tak ten kod ma sens -> algorytm wytłumaczony w raporcie gdzieś chyba?
    for (int slot = 0; slot < circle_size; slot++) {
        int res_type = slot % 3;
        int best_idx   = -1;
        int best_rank  = INT_MAX;
        uint32_t min_ratio  = UINT32_MAX;

        for (int i = 0; i < circle_size; i++) {
            if (assigned[i]) continue;

            uint8_t contributed = 0;
            uint8_t n = p->recv_def[i].num_parties;
            switch (res_type) {
                case 0: contributed = p->recv_def[i].sep;      break;
                case 1: contributed = p->recv_def[i].alko;     break;
                case 2: contributed = p->recv_def[i].zagrycha; break;
            }
            /* ratio = contributed * 1000 / num_parties (stałoprzecinkowe) */
            uint32_t ratio = (n > 0) ? ((uint32_t)contributed * 1000u / n)
                                     : 1000u;

            if (ratio < min_ratio ||
                (ratio == min_ratio && p->participants[i] < best_rank)) {
                min_ratio = ratio;
                best_idx  = i;
                best_rank = p->participants[i];
            }
        }

        if (best_idx != -1) {
            assigned[best_idx] = true;
            result[best_idx]   = (contribution_t)res_type;
        }
    }

    /* Wypisz wyniki i zapamiętaj swój wkład */
    LOG(p, "[DISPOSITION] Podział kółka:");
    for (int i = 0; i < circle_size; i++) {
        LOG(p, "[DISPOSITION]   P%d -> %s", p->participants[i],
            contrib_name[result[i]]);
        if (p->participants[i] == p->rank)
            p->what_i_bring = result[i];
    }

    /* Zaktualizuj własny deficit na przyszłe rundy */
    switch (p->what_i_bring) {
        case SEPIA:   p->my_def.sep++;      break;
        case ALKOHOL: p->my_def.alko++;     break;
        case ZAGRYCHA:p->my_def.zagrycha++; break;
    }
    LOG(p, "[DISPOSITION] Ja przynoszę: %s", contrib_name[p->what_i_bring]);
}


/* ============================================================
 * Handlery stanów (wszystkie nieblokujące — sprawdzają timery i
 * wracają natychmiast, jeśli jeszcze nie czas działać)
 * ============================================================ */

static void handle_kacuje(proc_t *p) {
    if (!p->ready_to_kac) return;

    if (!p->kac_in_progress) {
        p->ack_count         = 0;
        p->request_sent      = false;
        p->all_acks_received = false;
        p->acked_mask        = 0;
        p->is_organizer      = false;
        p->hello_count       = 0;
        p->hello_sent        = false;
        p->rel_received      = false;
        memset(p->participants, 0, sizeof(p->participants));

        p->my_def.num_parties++;
        uint32_t delay_ms = MIN_KAC_MS + (uint32_t)rand() % (MAX_KAC_MS - MIN_KAC_MS);
        p->kac_until      = MPI_Wtime() + MS_TO_SEC(delay_ms);
        p->kac_in_progress = true;

        LOG(p, "KACUJE: odpoczywam %u ms (impreza #%d)",
            delay_ms, p->my_def.num_parties);
    }

    if (MPI_Wtime() >= p->kac_until) {
        p->kac_in_progress = false;
        set_state(p, WYSYLAM_REQ);
    }
}

static void do_send_req(proc_t *p) {
    msg_t req = {
        .type = MSG_REQ,
        .from = p->rank,
        .ts   = p->request_ts,
    };
    broadcast(p, &req);
    p->req_sent_at = MPI_Wtime();
}

static void handle_wysylam_req(proc_t *p) {
    if (!p->request_sent) {
        p->request_ts   = ++p->lts;
        LOG(p, "WYSYLAM_REQ: wysyłam REQ[ts=%llu] do wszystkich",
            (unsigned long long)p->request_ts);
        do_send_req(p);
        p->request_sent = true;
        return;
    }

    /* Retransmisja jeśli nie zebrano wszystkich ACK w czasie */
    if (!p->all_acks_received &&
        (MPI_Wtime() - p->req_sent_at) > MS_TO_SEC(REQ_RETRY_MS)) {
        LOG(p, "WYSYLAM_REQ: timeout ACK (%d/%d), retransmit",
            p->ack_count, p->nprocs - 1);
        do_send_req(p);
    }
}

static void handle_jestem_w_kolejce(proc_t *p) {
    /* Poczekaj na komplet ACK — bez tego kolejka może być niespójna */
    if (!p->all_acks_received) return;

    int my_pos = -1;
    for (int i = 0; i < p->queue_size; i++) {
        if (p->queue[i].rank == p->rank) { my_pos = i; break; }
    }
    if (my_pos == -1) return;

    /* Co circle_size-ty w kolejce zostaje organizatorem */
    if ((my_pos + 1) % circle_size != 0) {
        /* Loguj pozycję tylko gdy kolejka jest wystarczająco duża */
        if (p->queue_size >= circle_size)
            LOG(p, "JESTEM_W_KOLEJCE: pozycja %d/%d", my_pos + 1, p->queue_size);
        return;
    }

    p->is_organizer = true;
    for (int i = 0; i < circle_size; i++)
        p->participants[i] = p->queue[my_pos - (circle_size - 1) + i].rank;

    /* Wypisz skład kółka */
    char kolko_buf[128] = "JESTEM ORGANIZATOREM! Kółko:";
    for (int i = 0; i < circle_size; i++) {
        char tmp[16];
        snprintf(tmp, sizeof(tmp), " P%d", p->participants[i]);
        strncat(kolko_buf, tmp, sizeof(kolko_buf) - strlen(kolko_buf) - 1);
    }
    LOG(p, "%s", kolko_buf);

    msg_t welcome = {
        .type    = MSG_WELCOME,
        .from    = p->rank,
        .ts      = ++p->lts,
        .deficit = p->my_def,
    };
    for (int i = 0; i < circle_size; i++)
        welcome.participants[i] = p->participants[i];

    for (int i = 0; i < circle_size; i++)
        if (p->participants[i] != p->rank)
            send_to(p->participants[i], &welcome);

    set_state(p, UMAWIAM_IMPREZE);
}

static void handle_umawiam_impreze(proc_t *p) {
    if (p->hello_sent) return;

    msg_t hello = {
        .type    = MSG_HELLO,
        .from    = p->rank,
        .ts      = ++p->lts,
        .deficit = p->my_def,
    };
    LOG(p, "UMAWIAM_IMPREZE: wysyłam HELLO");
    for (int i = 0; i < circle_size; i++)
        if (p->participants[i] != p->rank)
            send_to(p->participants[i], &hello);

    p->hello_sent = true;
}

static void handle_impreza(proc_t *p) {
    /* Pierwsze wywołanie — ustaw timer */
    if (p->party_until == 0.0) {
        p->party_until = MPI_Wtime() + MS_TO_SEC(PARTY_MS);
        LOG(p, "IMPREZA! Trwa %d ms...", PARTY_MS);
        return;
    }

    if (p->is_organizer) {
        if (MPI_Wtime() < p->party_until) return;

        /* Organizator wysyła REL do wszystkich po zakończeniu imprezy */
        msg_t rel = {
            .type = MSG_REL,
            .from = p->rank,
            .ts   = ++p->lts,
        };
        for (int i = 0; i < circle_size; i++)
            rel.participants[i] = p->participants[i];

        LOG(p, "IMPREZA skończona! Wysyłam REL");
        broadcast(p, &rel);

        /* Krótka pauza — daje uczestnikom czas na przetworzenie REL
         * zanim organizator wróci do kolejki z nowym REQ */
        usleep(REL_GRACE_MS * 1000);
        mqueue_remove(p, p->participants, circle_size);

    } else {
        /* Uczestnik czeka na REL od organizatora */
        if (!p->rel_received) return;
        LOG(p, "Otrzymałem REL, kończę IMPREZA");
    }

    p->parties_completed++;
    p->party_until = 0.0;
    set_state(p, KACUJE);
}

static void tick(proc_t *p) {
    switch (p->state) {
        case KACUJE:           handle_kacuje(p);           break;
        case WYSYLAM_REQ:      handle_wysylam_req(p);      break;
        case JESTEM_W_KOLEJCE: handle_jestem_w_kolejce(p); break;
        case UMAWIAM_IMPREZE:  handle_umawiam_impreze(p);  break;
        case IMPREZA:          handle_impreza(p);          break;
    }
}



static void on_message(proc_t *p, const msg_t *m) {
    /* Aktualizacja zegara Lamporta */
    p->lts = MAX(p->lts, (uint64_t)m->ts) + 1;

    /* ---------- MSG_READY: bariera startowa ---------- */
    if (m->type == MSG_READY) {
        p->ready_count++;
        LOG(p, "MSG_READY od P%d (%d/%d)",
            m->from, p->ready_count, p->nprocs - 1);
        if (p->ready_count >= p->nprocs - 1 && !p->ready_to_kac) {
            p->ready_to_kac = true;
            LOG(p, "*** BARIERA OSIAGNIĘTA — wszystkie procesy gotowe! ***");
        }
        return;
    }

    /* Jeśli dostajemy jakąkolwiek inną wiadomość, ktoś już przeszedł
     * przez barierę — zdejmujemy ją wcześniej żeby nie blokować. */
    if (!p->ready_to_kac) {
        LOG(p, "Zdejmuję barierę (odebrano wiadomość type=%d)", m->type);
        p->ready_to_kac = true;
    }

    /* ---------- MSG_REQ: każdy odpowiada ACK i wstawia do kolejki ---------- */
    if (m->type == MSG_REQ) {
        LOG(p, "MSG_REQ od P%d [ts=%llu]",
            m->from, (unsigned long long)m->ts);
        mqueue_insert(p, m->from, m->ts);

        msg_t ack = { .type = MSG_ACK, .from = p->rank, .ts = ++p->lts };
        send_to(m->from, &ack);
        return;
    }

    /* ---------- Obsługa zależna od stanu ---------- */
    switch (p->state) {

        case WYSYLAM_REQ:
            if (m->type == MSG_ACK) {
                int sender = m->from;
                /* Zabezpieczenie przed zduplikowanymi ACK */
                if (sender < 64 && (p->acked_mask & (1ULL << sender))) break;
                if (sender < 64) p->acked_mask |= (1ULL << sender);
                p->ack_count++;
                LOG(p, "MSG_ACK od P%d (%d/%d)",
                    sender, p->ack_count, p->nprocs - 1);

                if (p->ack_count == p->nprocs - 1) {
                    /* Zebrano wszystkie ACK — wstawiamy siebie do kolejki */
                    mqueue_insert(p, p->rank, p->request_ts);
                    p->all_acks_received = true;
                    set_state(p, JESTEM_W_KOLEJCE);
                }
            }
            break;

        /* MSG_WELCOME może dotrzeć w obu stanach — obsługa identyczna */
        case JESTEM_W_KOLEJCE:
        case UMAWIAM_IMPREZE:
            if (m->type == MSG_WELCOME) {
                LOG(p, "MSG_WELCOME od P%d (jestem uczestnikiem)", m->from);

                p->hello_count = 0;
                p->hello_sent  = false;
                memset(p->recv_def, 0, sizeof(p->recv_def));

                for (int i = 0; i < circle_size; i++) {
                    p->participants[i] = m->participants[i];
                    /* Zapamiętaj deficit organizatora */
                    if (m->participants[i] == m->from)
                        p->recv_def[i] = m->deficit;
                }
                set_state(p, UMAWIAM_IMPREZE);

            } else if (m->type == MSG_HELLO && p->state == UMAWIAM_IMPREZE) {
                p->hello_count++;
                LOG(p, "MSG_HELLO od P%d (%d/%d)",
                    m->from, p->hello_count, circle_size - 1);

                for (int i = 0; i < circle_size; i++) {
                    if (p->participants[i] == m->from) {
                        p->recv_def[i] = m->deficit;
                        break;
                    }
                }

                if (p->hello_count == circle_size - 1) {
                    /* Uzupełnij swój własny deficit w tablicy */
                    for (int i = 0; i < circle_size; i++) {
                        if (p->participants[i] == p->rank) {
                            p->recv_def[i] = p->my_def;
                            break;
                        }
                    }
                    LOG(p, "Wszyscy wysłali HELLO — obliczam podział zasobów");
                    disposition(p);
                    set_state(p, IMPREZA);
                }
            }
            break;

        case IMPREZA:
            if (m->type == MSG_REL && !p->is_organizer) {
                LOG(p, "MSG_REL od P%d — impreza się kończy", m->from);
                mqueue_remove(p, m->participants, circle_size);
                p->rel_received = true;
            }
            break;

        default:
            break;
    }
}


// były mainnnnnnn ws to po prosu też main 
int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    proc_t p = {0};
    MPI_Comm_rank(MPI_COMM_WORLD, &p.rank);
    MPI_Comm_size(MPI_COMM_WORLD, &p.nprocs);

    //to co pisałem wyżej, oczyt circle size z środowuska i tak 
    const char *env_cs = getenv("CIRCLE_SIZE");
    if (env_cs != NULL) {
        circle_size = atoi(env_cs);
    }
    if (circle_size < 2 || circle_size > MAX_CIRCLE_SIZE) {
        if (p.rank == 0)
            fprintf(stderr,
                "Błąd: CIRCLE_SIZE=%d poza zakresem [2, %d].\n",
                circle_size, MAX_CIRCLE_SIZE);
        MPI_Finalize();
        return 1;
    }
    if (p.nprocs < circle_size) {
        if (p.rank == 0)
            fprintf(stderr,
                "Błąd: potrzeba co najmniej %d procesów (uruchomiono %d).\n"
                "Użyj: CIRCLE_SIZE=%d mpirun -np %d ./impreza\n",
                circle_size, p.nprocs, circle_size, circle_size);
        MPI_Finalize();
        return 1;
    }

    //cos czego o czym zapomnielismy we wersji ESP32 XDDDDDDD, dlatego te wszyskie randomy były takie same per urządzenie, bo seedy były też takie same XDDDDDDDDDd
    srand((unsigned)(p.rank * 12345 + 9999));

    p.state = KACUJE;

    printf("[P%02d] Start. Procesów: %d, CIRCLE_SIZE: %d, MAX_PARTIES: %d\n",
           p.rank, p.nprocs, circle_size, MAX_PARTIES);
    fflush(stdout);

    /* Bariera MPI — wszyscy zaczynają jednocześnie */
    MPI_Barrier(MPI_COMM_WORLD);

    /* Wyślij MSG_READY do wszystkich rówieśników */
    msg_t ready = { .type = MSG_READY, .from = p.rank, .ts = ++p.lts };
    broadcast(&p, &ready);
    LOG(&p, "Wysłano MSG_READY, czekam na pozostałe procesy...");

    // były main task, teraz po prostu główny loop
     while (p.parties_completed < MAX_PARTIES) {
        //odbieraj wszyskie wiadoosci jakie sa na kanałach 
        msg_t m;
        while (poll_msg(&m))
            on_message(&p, &m);

        tick(&p);
        usleep(TICK_MS * 1000);
    }

    LOG(&p, "DEBUG Ukończyłem %d imprez ", MAX_PARTIES);


    double drain_until = MPI_Wtime() + MS_TO_SEC(MAX_KAC_MS + PARTY_MS + 5000);
    while (MPI_Wtime() < drain_until) {
        msg_t m;
        while (poll_msg(&m)) {
            /* Odpowiadaj na REQ (ACK) żeby nie blokować kolejki innych */
            if (m.type == MSG_REQ) {
                p.lts = MAX(p.lts, (uint64_t)m.ts) + 1;
                msg_t ack = { .type = MSG_ACK, .from = p.rank, .ts = ++p.lts };
                send_to(m.from, &ack);
            }
        }
        usleep(TICK_MS * 1000);
    }

    printf("[P%02d] Finalizuję MPI. Łącznie imprez: %d\n",
           p.rank, p.parties_completed);
    fflush(stdout);

    MPI_Finalize();
    return 0;
}
