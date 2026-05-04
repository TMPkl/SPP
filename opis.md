# Stowarzyszenie Upitych Artystów
## Specyfikacja algorytmu synchronizacji kółek poetyckich

---

## Treść zadania

W pewnym mieście uniwersyteckim grupa młodych ludzi wpadła na pomysł, by zwiększyć swoje zdolności twórcze konsumpcją dużej ilości alkoholu. W tym celu tworzą kółka poetyckie, po czym każdy przynosi alkohol lub zagrychę, przeprowadzają libację, przy okazji usiłując spłodzić dzieła mające przebić Przybyszewskiego. Po libacji poeta jakiś czas odpoczywa. Poeci działają pod natchnieniem chwili, więc mogą zdecydować w sposób losowy, że do jakiegoś kółka nie chcą przynależeć.

Danych jest P poetów. Poeci dobierają się w kółka o wielkości K. Po zebraniu w kółko poeci decydują, kto przyniesie alkohol, a kto zagrychę, a kto może pić na sępa. Poeci nie lubią być frajerami, więc nie mogą non stop przynosić jednego rodzaju towaru lub sępić. Po libacji poeci odpoczywają (i mają różną odporność na alkohol, więc każdy może odpoczywać inny czas).

---

## Algorytm i struktury danych

Do rozwiązania zadania wykorzystany zostanie **zmodyfikowany algorytm Lamporta**. Główną strukturą danych przechowywaną przez każdy proces będzie kolejka `QUEUE`, do której zapis będzie się odbywał poprzez wysłanie wiadomości `REQ` do każdego procesu, aby proces widział sam siebie w kolejce musi na początku odebrać od wszystkich `ACK`. Miejsce w kolejce będzie zapewnianie kolejno przez `ts` a następnie adres MAC urządzenia, co zapewni determinizm w jego przydziale i da nam pewność, że kolejki wszystkich procesów będą wyglądały tak samo. 

Każde kolejne K miejsc w kolejce to będą nasze kółka. Kółko zostaje założony przez procesy które są na miejscu w kolejce będącym(wielokrotnością liczby K) - 1. Taki proces staje się organizatorem kółka, wysyłając wiadomość `WELCOME` będącą wiadomością tworzącą kółko, oraz `REL` będącą wiadomością rozwiązującą kółko po końcu imprezy. 

Procesy są usuwane z kolejki dopiero po zakończeniu imprezy i wysłaniu wiadomości `REL` przez organizatora. Kolejka jest więc strukturą zarówno dla procesów oczekujących na sekcję krytyczną jak i tych już będących w sekcji krytycznej. Dzięki temu możemy kontrolować liczbę sekcji krytycznych. 

## Wiadomości

| Wiadomość | Pola | Opis |
|---|---|---|
| `REQ(from, ts)` | id procesu wysyłającego, znacznik czasowy zegaru Lamporta | wysyłane gdy proces chce dołączyć do kolejki |
| `ACK(from, ts)` | id procesu wysyłającego, znacznik czasowy zegaru Lamporta | potwierdzenie odbioru wiadomości |
| `REL(from, ts)` | id procesu wysyłającego, znacznik czasowy zegaru Lamporta | zwolnienie sekcji krytycznej i miejsc w kolejce |
| `WELCOME(from, ts, deficits, participants)` | id procesu wysyłającego, znacznik czasowy zegaru Lamporta, tablica deficytów dotychczasowych przynoszonych rzeczy, tablica uczestników koła | wiadomość wysyłana przez organizatora kółka informująca o założeniu kółka procesy w kółku |
| `HELLO(from, ts, deficits)` | id procesu wysyłającego, znacznik czasowy zegaru Lamporta, tablica deficytów dotychczasowych przynoszonych rzeczy | powitanie uczestników kółka służącego jednocześnie do dogadania się co kto przynosi |
| `REL(from, ts, participants)` | id procesu wysyłającego, znacznik czasowy zegaru Lamporta, tablica uczestników usuwanego koła | wiadomość wysyłana przez organizatora kółka aby zwolnić sekcję krytyczną i usunąć z kolejki wszystkie procesy w rozwiązywanym kółku

---

## Stany

**UWAGA**: W każdym stanie, każdy proces obsługuje przychodzące `REQ` odpowiadając na nie `ACK` i aktualizując stale kolejkę.

| Stan | Opis |
|---|---|
| **KACUJE** | stan odpoczynku po imprezie, proces czeka losową ilość czasu |
| **WYSYLAM_REQ** | proces wysyła do wszystkich innych procesów wiadomość `REQ` a następnie czeka na `ACK` od wszystkich aby móc umieścić siebie w kolejce i przejść do stanu **JESTEM_W_KOLEJCE** |
| **JESTEM_W_KOLEJCE** | proces sprawdza swoje miejsce w kolejce, jeśli jest K-tym elementem to zakłada kółko wysyłając do swoich K-1 poprzedników wiadomość `WELCOME`, jeśli nie to na nią oczekuje aby móc przejść do kolejnego stanu |
| **UMAWIAM_IMPREZE** | w tym stanie wszystkie procesu oprócz procesu organizatora wysyłają do całego koła `HELLO` a następnie gdy otrzymają od wszystkich innych oprócz organizatora wiadomość `HELLO` to obliczają u siebie na podstawie ustalonego algorytmu co kto przynosi. Po obliczeniu przechodzą w stan **IMPREZA** |
| **IMPREZA** | impreza trwa ustaloną ilość czasu, gdy się skończy organizator wysyła broadcast `REL` informując o zwolnieniu miejsc w kolejce przez wszystkie procesy w kole i przechodzi do kolejnego stanu. Uczestnicy po skończeniu czasu czekają na `REL` od organizatora i następnie przechodzą do stanu **KACUJE** |


> **Diagram stanow:** KACUJE -> WYSYLAM_REQ -> JESTEM_W_KOLEJCE -> UMAWIAM_IMPREZE -> IMPREZA -> KACUJE 

### Algorytm podziału zasobów między procesy w stanie UMAWIAM_IMPREZE

Każdy proces uczestnika koła posiada ten sam algorytm do obliczania co kto ma przynieść, następnie notuje swój wynik na przyszłość. 

Podział zasobów występuje zgodnie z konwencją: 

| K mod 3 | sęp | alkohol | zagrycha |
|---|---|---|---|
| 0 | K/3 | K/3 | K/3 |
| 1 | K/3 + 1 | K/3 | K/3 |
| 2 | K/3 + 1 | K/3 + 1 | K/3 |

Zapewnia to równą liczbę każdej roli w kole. 

Aby przydzielić zasoby każdy proces w wiadomości `HELLO` wysyła swoją tablicę deficytów (jej parametry są liczone jako $\frac{ileRazyPrzynioslemZasob}{iloscImprezWKtorychBralemUdzial}$). Obliczanie kto co przynosi odbywa się za pomocą pętli iterującej po wszystkich zasobach, i sprawdzającej dla każdego miejsca w nich deficyty uczestników w tej kategorii. Przypisany do miejsca zostaje uczestnik o największym deficycie.

