# Stowarzyszenie Upitych Artystów
## Specyfikacja algorytmu synchronizacji kółek poetyckich

---

## Treść zadania

W pewnym mieście uniwersyteckim grupa młodych ludzi wpadła na pomysł, by zwiększyć swoje zdolności twórcze konsumpcją dużej ilości alkoholu. W tym celu tworzą kółka poetyckie, po czym każdy przynosi alkohol lub zagrychę, przeprowadzają libację, przy okazji usiłując spłodzić dzieła mające przebić Przybyszewskiego. Po libacji poeta jakiś czas odpoczywa. Poeci działają pod natchnieniem chwili, więc mogą zdecydować w sposób losowy, że do jakiegoś kółka nie chcą przynależeć.

Danych jest P poetów. Poeci dobierają się w kółka o wielkości K. Po zebraniu w kółko poeci decydują, kto przyniesie alkohol, a kto zagrychę, a kto może pić na sępa. Poeci nie lubią być frajerami, więc nie mogą non stop przynosić jednego rodzaju towaru lub sępić. Po libacji poeci odpoczywają (i mają różną odporność na alkohol, więc każdy może odpoczywać inny czas).

---

## Struktury i zmienne

### Parametry globalne

| Parametr | Opis |
|---|---|
| `P` | Liczba wszystkich poetów |
| `K` | Pojemność kółka |
| `oczekiwalność` | Czas mierzony od zakończenia **ODPOCZYNEK**, po przekroczeniu którego poeta próbuje organizować kółko; wartość losowa, parametryzowana |

### Zmienne lokalne każdego procesu

| Zmienna | Typ | Opis |
|---|---|---|
| `my_ID` | int | Unikalny identyfikator poety, nadawany podczas inicjalizacji |
| `my_clock` | int | Zegar Lamporta |
| `co_przynoszę` | bool[3] | Rola w bieżącym kółku: `[sęp, alkohol, zagrycha]`; dokładnie jedno `true` po uzgodnieniu |
| `co_przynosiłem` | int[3] | Historia ról: `[sęp, alkohol, zagrycha]`; zliczenie udziałów w kolejnych kółkach; indeksy: `0=sęp, 1=alkohol, 2=zagrycha`; inicjalizowane na `[0, 0, 0]` |
| `obrażony_na_koło` | bool[] | Tablica długości `round_up(P/K)`; czy poeta odmawia udziału w  kółkach; wartości zmieniają się losowo |
| `my_round_id` | (int, int) | ID aktualnego kółka = `(my_ID, my_clock)` w chwili wysłania REQUEST; gwarantuje unikalność |
| `organizing` | bool | Czy jestem aktualnie organizatorem kółka |
| `in_round` | bool | Czy jestem w aktywnym kółku |
| `request_queue` | lista | Kolejka odebranych REQUEST-ów, sortowana po `(ts, ID)` |
| `pending_replies` | int | Liczba oczekiwanych REPLY na mój REQUEST |
| `collected_oks` | lista | Poeci którzy odpowiedzieli `OK` na mój REQUEST |
| `deferred_requests` | lista | REQUEST-y odłożone do późniejszej odpowiedzi (gdy sam jestem w REQUESTING z wyższym priorytetem) |
| `participants` | lista | Uczestnicy bieżącego kółka (bez organizatora); używana w stanach WYSYLAM_ZAPRO, POWITANIA, IMPREZZAAA |
| `deficyty_uczestnikow` | map: ID → int[3] | Zebrane wektory deficytów od wszystkich uczestników kółka (w stanie POWITANIA) |

---

## Wiadomości

| Wiadomość | Pola | Opis |
|---|---|---|
| `REQUEST(ts, from)` | znacznik czasu Lamporta, ID nadawcy | Ogłoszenie chęci organizacji kółka |
| `REPLY(ts, from, status)` | znacznik czasu, ID, status in {OK, BUSY, OBRAŻONY} | Odpowiedź na REQUEST |
| `INVITE(round_id, from)` | ID kółka, ID organizatora | Zaproszenie do konkretnego kółka |
| `IM_INTERESTED(round_id, from)` | ID kółka, ID akceptującego | Potwierdzenie chęci udziału |
| `WELCOME(round_id, participants[])` | ID kółka, lista uczestników | Potwierdzenie zapisu do kółka; wysyłane przez organizatora do wszystkich przyjętych |
| `YOURE_NOT_IN(round_id)` | ID kółka | odmowa zapisu dla członków którzy zaakceptowali ale się już nie zmieścili do kółka |
| `DECLINE(round_id, from)` | ID kółka, ID odmawiającego | Odmowa (obrażony lub zajęty) |
| `HELLO(round_id, from, deficyty[])` | ID kółka, ID nadawcy, tablica `int[3]` | Każdy uczestnik wysyła pozostałym swój wektor `co_przynosiłem[]` (historię ról) |
| `RELEASE(round_id, from)` | ID kółka, ID nadawcy | Koniec libacji, zwolnienie zasobu |

---

## Stany

| Stan | Opis |
|---|---|
| **ODPOCZYNEK** | Stan początkowy; poeta odpoczywa po libacji przez losowy czas |
| **REQUESTING** | Naszło mnie organizować libację; rozsyłam REQUEST i czekam na odpowiedzi |
| **WYSYLAM_ZAPRO** | Mam sekcję krytyczną; zapraszam chętnych poetów i kompletuję skład kółka |
| **WAITING_FOR_WELCOME** | Czekam aż organizator potwierdzi mi że dostałem się do kółka |
| **POWITANIA** | Jestem w kółku; wymieniam deficyty z pozostałymi uczestnikami i ustalam swoją rolę |
| **IMPREZZAAA** | Libacja trwa |
| **OBRAZILEM_SIE** | Losowo stwierdziłem że nie chcę brać udziału w kółku przez jakiś czas |

> **Stan początkowy:** ODPOCZYNEK

---

## Opis szczegółowy algorytmu dla procesu i

### ODPOCZYNEK -> REQUESTING

Po upłynięciu czasu `oczekiwalności` od końca ostatniej libacji (lub startu procesu):

```
my_clock++
my_round_id = (my_ID, my_clock)
Rozgłoś REQUEST(my_clock, my_ID) do wszystkich poetów
pending_replies = P - 1   // liczba pozostałych poetów
collected_oks   = []
Przejdź do stanu REQUESTING
```

---

### Obsługa odebranego REQUEST(ts, from) - w dowolnym stanie

```
my_clock = max(my_clock, ts) + 1

if stan == OBRAZILEM_SIE:
    odpowiedz REPLY(my_clock, my_ID, OBRAŻONY)

if stan ∈ {IMPREZZAAA, WYSYLAM_ZAPRO, POWITANIA, WAITING_FOR_WELCOME}:
    odpowiedz REPLY(my_clock, my_ID, BUSY)

if stan == REQUESTING:
    // Ricart-Agrawala
    if (ts < mój_ts) lub (ts == mój_ts i from < my_ID):
        // from ma wyższy priorytet 
        odpowiedz REPLY(my_clock, my_ID, OK)
    w przeciwnym razie:
        // ja mam wyższy priorytet 
        dodaj (ts, from) do deferred_requests[]

if stan == ODPOCZYNEK:
    odpowiedz REPLY(my_clock, my_ID, OK)
```

 Odpowiedź `OK` oznacza wyłącznie „nie blokuję cię w tworzeniu kółka" - nie jest jeszcze deklaracją uczestnictwa.

---

### Obsługa odebranego REPLY(ts, from, status) - będąc w REQUESTING

```
my_clock = max(my_clock, ts) + 1
pending_replies--

if status == OK:
    collected_oks.append(from)

if pending_replies == 0:
    // Zebrałem wszystkie odpowiedzi - wchodzę w sekcję krytyczną
    organizing = true
    in_round = true
    participants = []
    Przejdź do WYSYLAM_ZAPRO
```

---

### Stan WYSYLAM_ZAPRO

Organizator ma wyłączne prawo do ogłaszania kółka (w sensie Ricart-Agrawala). Zaprasza poetów spośród tych, którzy odpowiedzieli `OK`:

```
kandydaci = collected_oks
Rozgłoś INVITE(my_round_id, my_ID) do wszystkich kandydatów

dopóki nie zebrano K-1 IM_INTERESTED-ów i są jeszcze kandydaci:
    na IM_INTERESTED(round_id, from):
        dodaj (from) do participants[]
    na DECLINE(round_id, from):
        usuń (from) z kandydatów

jeśli |participants| == K-1:
    // Komplet uczestników → potwierdzenia
    Wyślij WELCOME(my_round_id, participants ∪ {my_ID}) do każdego w participants[]
    deficyty_uczestnikow = {}
    Przejdź do POWITANIA
w przeciwnym razie:
    // Za mało chętnych → kółko nie dochodzi do skutku
    in_round   = false
    organizing = false
    Zwolnij sekcję krytyczną (patrz procedura RELEASE niżej)
    Przejdź do ODPOCZYNEK
```

---

### Obsługa odebranego INVITE(round_id, from) - będąc w ODPOCZYNEK

```
if in_round == false i stan != OBRAZILEM_SIE:
    in_round    = true
    my_round_id = round_id
    odpowiedz IM_INTERESTED(round_id, my_ID)
    Przejdź do WAITING_FOR_WELCOME
w przeciwnym razie:
    odpowiedz DECLINE(round_id, my_ID)
```

---

### Stan WAITING_FOR_WELCOME

Zaproszony poeta czeka po wysłaniu IM_INTERESTED na potwierdzenie lub odrzucenie przez organizatora. W tym czasie traktuje wszelkie nowe INVITE jak będąc zajętym (`in_round == true` powoduje DECLINE).

```
na WELCOME(round_id, participants[]):
    deficyty_uczestnikow = {}
    Rozgłoś HELLO(my_round_id, my_ID, co_przynosiłem[]) do wszystkich w participants[] \ {my_ID}
    Przejdź do POWITANIA

na YOURE_NOT_IN(round_id):
    in_round = false
    Przejdź do ODPOCZYNEK
```

### Stan POWITANIA

Każdy uczestnik (w tym organizator) rozsyła swój wektor historii ról do pozostałych, zbiera ich wektory, po czym **samodzielnie** oblicza przydzieloną sobie rolę.

#### Wymiana deficytów

**Przy wejściu do stanu** (dotyczy zarówno organizatora, jak i uczestników po odebraniu WELCOME):

```
Rozgłoś HELLO(my_round_id, my_ID, co_przynosiłem[]) do wszystkich pozostałych w kółku
deficyty_uczestnikow[my_ID] = co_przynosiłem[]   // zapisz własne dane
```

**Obsługa odebranego HELLO(round_id, from, deficyty[]):**

```
deficyty_uczestnikow[from] = deficyty[]

jeśli |deficyty_uczestnikow| == K:   // zebrano dane od wszystkich K uczestników
    co_przynoszę = oblicz_moją_rolę(deficyty_uczestnikow, my_ID)
    Przejdź do IMPREZZAAA
```


**Metryka deficytu** 

Dla poety `i` i roli `r` deficyt wyraża, o ile mniej niż „sprawiedliwy udział" dana rola była przez poetę pełniona:

```
total_i      = co_przynosiłem[i][0] + co_przynosiłem[i][1] + co_przynosiłem[i][2]
deficit(i,r) = (1/3) * total_i  −  co_przynosiłem[i][r]
```

Im większy deficyt, tym bardziej poeta „zalega" z daną rolą.

**Liczba miejsc na każdą rolę** (konwencja że ma być róna ilość każdej roli w kole, anie że wystarczy jedna osoba z piciem i jedna z zagrychą):

| K mod 3 | sęp | alkohol | zagrycha |
|---|---|---|---|
| 0 | K/3 | K/3 | K/3 |
| 1 | K/3 + 1 | K/3 | K/3 |
| 2 | K/3 + 1 | K/3 + 1 | K/3 |

**Procedura przydziału:**

```
Wejście:  deficyty_uczestnikow — mapa ID → co_przynosiłem[] dla wszystkich K uczestników
Wejście:  my_ID
Wyjście:  rola przypisana procesowi my_ID

nieprzydzieleni = lista wszystkich K ID uczestników

dla każdej roli r w kolejności [sęp(0), alkohol(1), zagrycha(2)]:
    posortuj nieprzydzieleni malejąco po deficit(i, r),
        przy remisach rosnąco po ID (gwarantuje identyczny wynik u wszystkich)
    przydziel rolę r pierwszym miejsca_na_rolę[r] poetom z posortowanej listy
    usuń przydzielonych z nieprzydzieleni

zwróć rolę przypisaną my_ID
```

Ponieważ każdy proces dysponuje identycznym zestawem danych `deficyty_uczestnikow` i stosuje ten sam deterministyczny algorytm, każdy niezależnie dochodzi do tego samego przydziału — **bez konieczności wymiany dodatkowych wiadomości**.

**Dzięki założeniu o równości ról, i zastosowania algorytmu:** po dostatecznie wielu libacjach dla każdego poety `i`:

```
|co_przynosiłem[i][r] / total_i  −  1/3|  zmierza do  0
```

---

### Stan IMPREZZAAA

```
Libacja trwa przez losowy czas
Po zakończeniu:
    r = co_przynoszę
    co_przynosiłem[r]++
    co_przynoszę = -1

    Wyślij RELEASE(my_round_id, my_ID) do wszystkich w kółku

    // Zwolnienie sekcji krytycznej R-A 
    if organizing == true:
        dla każdego (ts, from) w deferred_requests[]: 
            wyślij REPLY(my_clock, my_ID, OK) do from
        deferred_requests = []
        organizing        = false

    in_round = false
    Przejdź do ODPOCZYNEK
```

---

### Stan OBRAZILEM_SIE

```
 Wejście: losowo z ODPOCZYNEK, na losowy czas
 W tym czasie:
  - na REQUEST odpowiadaj REPLY(..., OBRAŻONY)
   - na INVITE  odpowiadaj DECLINE(...)
 Po upływie czasu:
Przejdź do ODPOCZYNEK
```

---
