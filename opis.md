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
| `obrażony_na_koło` | bool[] | Tablica długości `⌈P/K⌉`; czy poeta odmawia udziału w kolejnych kółkach; wartości zmieniają się losowo |
| `my_round_id` | (int, int) | ID aktualnego kółka = `(my_ID, my_clock)` w chwili wysłania REQUEST; gwarantuje unikalność |
| `organizing` | bool | Czy jestem aktualnie organizatorem kółka |
| `in_round` | bool | Czy jestem w aktywnym kółku |
| `request_queue` | lista | Kolejka odebranych REQUEST-ów, sortowana po `(ts, ID)` |
| `pending_replies` | int | Liczba oczekiwanych REPLY na mój REQUEST |
| `collected_oks` | lista | Poeci którzy odpowiedzieli `OK` na mój REQUEST |
| `deferred_requests` | lista | REQUEST-y odłożone do późniejszej odpowiedzi (gdy sam jestem w REQUESTING z wyższym priorytetem) |

---

## Wiadomości

| Wiadomość | Pola | Opis |
|---|---|---|
| `REQUEST(ts, from)` | znacznik czasu Lamporta, ID nadawcy | Ogłoszenie chęci organizacji kółka |
| `REPLY(ts, from, status)` | znacznik czasu, ID, status in {OK, BUSY, OBRAŻONY} | Odpowiedź na REQUEST |
| `INVITE(round_id, from)` | ID kółka, ID organizatora | Zaproszenie do konkretnego kółka |
| `ACCEPT(round_id, from, co_przynosiłem[])` | ID kółka, ID akceptującego, historia ról | Potwierdzenie udziału + dane do przydziału ról |
| `DECLINE(round_id, from)` | ID kółka, ID odmawiającego | Odmowa (obrażony lub zajęty) |
| `ASSIGN(round_id, przydziały[])` | ID kółka, tablica `{poet_id → rola}` | Organizator rozgłasza kto co przynosi |
| `ACK_ASSIGN(round_id, from)` | ID kółka, ID potwierdzającego | Potwierdzenie odebrania przydziału |
| `RELEASE(round_id, from)` | ID kółka, ID nadawcy | Koniec libacji, zwolnienie zasobu |

---

## Stany

| Stan | Opis |
|---|---|
| **ODPOCZYNEK** | Stan początkowy; poeta odpoczywa po libacji przez losowy czas |
| **REQUESTING** | Naszło mnie organizować libację; rozsyłam REQUEST i czekam na odpowiedzi |
| **WYSYLAM_ZAPRO** | Mam sekcję krytyczną; zapraszam K-1 chętnych poetów |
| **KTO_CO_PRZYNOSI** | Jestem w kółku; ustalamy kto co przynosi |
| **IMPREZZAAA** | Libacja trwa |
| **OBRAZILEM_SIE** | Losowo stwierdziłem że nie chcę brać udziału w kółku przez jakiś czas |

> **Stan początkowy:** ODPOCZYNEK

---

## Opis szczegółowy algorytmu dla procesu i

### ODPOCZYNEK → REQUESTING

Po upłynięciu czasu `oczekiwalności` od końca ostatniej libacji (lub startu procesu):

```
my_clock++
my_round_id = (my_ID, my_clock)
Rozgłoś REQUEST(my_clock, my_ID) do wszystkich poetów
pending_replies = liczba poetów do których wysłałem REQUEST 
collected_oks   = []
Przejdź do stanu REQUESTING
```

---

### Obsługa odebranego REQUEST(ts, from) — w dowolnym stanie

```
my_clock = max(my_clock, ts) + 1

if stan == OBRAZILEM_SIE:
     REPLY(my_clock, my_ID, OBRAŻONY)

if stan  {IMPREZZAAA, WYSYLAM_ZAPRO, KTO_CO_PRZYNOSI}:
     REPLY(my_clock, my_ID, BUSY)

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

 Odpowiedź `OK` oznacza wyłącznie „nie blokuję cię w tworzeniu kółka" — nie jest jeszcze deklaracją uczestnictwa.

---

### Obsługa odebranego REPLY(ts, from, status) — będąc w REQUESTING

```
my_clock = max(my_clock, ts) + 1
pending_replies--

if status == OK:
    collected_oks.append(from)

if pending_replies == 0:
    // Zebrałem wszystkie odpowiedzi — wchodzę w sekcję krytyczną
    Przejdź do WYSYLAM_ZAPRO
```

---

### Stan WYSYLAM_ZAPRO

Organizator ma wyłączne prawo do ogłaszania kółka (w sensie Ricart-Agrawala). Zaprasza poetów spośród tych, którzy odpowiedzieli `OK`:

```
kandydaci = collected_oks
Rozgłoś INVITE(my_round_id, my_ID) do wszystkich kandydatów
Ustaw timeout oczekiwania na ACCEPT / DECLINE

dopóki nie zebrano K-1 ACCEPT-ów i są jeszcze kandydaci:
    na ACCEPT(round_id, from, co_przynosiłem[]):
        dodaj (from, co_przynosiłem[]) do składu_kółka
    na DECLINE(round_id, from):
        usuń from z kandydatów

if |skład_kółka| >= K-1:
    // Mam komplet — przechodzę do negocjacji ról
    Przejdź do KTO_CO_PRZYNOSI
w przeciwnym razie:
    // Za mało chętnych — kółko nie dochodzi do skutku
    Zwolnij sekcję krytyczną (wyślij odłożone REPLY-e, patrz: RELEASE)
    Przejdź do ODPOCZYNEK
```

---

### Obsługa odebranego INVITE(round_id, from) — będąc w ODPOCZYNEK

```
if in_round == false i stan != OBRAZILEM_SIE:
    in_round    = true
    my_round_id = round_id
    odpowiedz ACCEPT(round_id, my_ID, co_przynosiłem[])
    Przejdź do KTO_CO_PRZYNOSI
w przeciwnym razie:
    odpowiedz DECLINE(round_id, my_ID)
```

---

### Stan KTO_CO_PRZYNOSI

#### Przydział ról — algorytm deficytowy

Organizator posiada teraz wektory `co_przynosiłem[]` wszystkich K uczestników (zebrane z wiadomości `ACCEPT` + własne dane). Stosuje **deficit-based scheduling**:

**Metryka deficytu** dla poety `i` i roli `r`:

```
total_i      = sęp_i + alkohol_i + zagrycha_i
deficit(i,r) = (1/3) * total_i  -  co_przynosiłem[i][r]
```

Im większy deficyt, tym bardziej poeta „zalega" z daną rolą.

**Liczba miejsc na każdą rolę** (konwencja deterministyczna):

| K mod 3 | sęp | alkohol | zagrycha |
|---|---|---|---|
| 0 | K/3 | K/3 | K/3 |
| 1 | K/3 + 1 | K/3 | K/3 |
| 2 | K/3 + 1 | K/3 + 1 | K/3 |

**Procedura przydziału:**

```
Wejście:  skład[] — K poetów z ich co_przynosiłem[]
Wyjście:  przydziały{poet_id → rola}

nieprzydzieleni = skład[]

dla każdej roli r w kolejności [sęp, alkohol, zagrycha]:
    posortuj nieprzydzieleni malejąco po deficit(i, r)
    przydziel rolę r pierwszym miejsca_na_rolę[r] poetom z listy
    usuń przydzielonych z nieprzydzieleni
```

**Właściwość gwarantowana:** po dostatecznie wielu libacjach dla każdego poety `i`:

```
|co_przynosiłem[i][r] / total_i  −  1/3|  →  0
```

#### Przebieg stanu

**Organizator:**

```
przydziały = oblicz_przydział(skład_kółka ∪ {my_ID})
Rozgłoś ASSIGN(my_round_id, przydziały) do wszystkich w kółku
Czekaj na ACK_ASSIGN od każdego członka kółka
Po zebraniu wszystkich ACK → Przejdź do IMPREZZAAA
```

**Uczestnik:**

```
Na odebranie ASSIGN(round_id, przydziały):
    co_przynoszę = przydziały[my_ID]   // dokładnie jedno true
    odpowiedz ACK_ASSIGN(round_id, my_ID)
    Przejdź do IMPREZZAAA
```

---

### Stan IMPREZZAAA

```
// Libacja trwa przez losowy czas
Po zakończeniu:
    r                      = indeks roli gdzie co_przynoszę[r] == true
    co_przynosiłem[r]++
    co_przynoszę - wyzerować na przyszłość

    Wyślij RELEASE(my_round_id, my_ID) do wszystkich w kółku

    // Zwolnienie sekcji krytycznej R-A 
    if organizing == true:
        dla każdego (ts, from) w deferred_requests[]: //chodzi o to że dajemy znać tym co ich blokujemy że już nie bolokujemy
            wyślij REPLY(my_clock, my_ID, OK) do from
        deferred_requests = []
        organizing        = false

    in_round = false
    Przejdź do ODPOCZYNEK
```

---

### Stan OBRAZILEM_SIE

```
// Wejście: losowo z ODPOCZYNEK, na losowy czas
// W tym czasie:
//   - na REQUEST odpowiadaj REPLY(..., OBRAŻONY)
//   - na INVITE  odpowiadaj DECLINE(...)
// Po upływie czasu:
Przejdź do ODPOCZYNEK
```

---
