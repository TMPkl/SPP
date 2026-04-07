### Stowarzyszenie upitych artystów 

Struktury i zmienne:
- zmienne globalne/ parametry:
    - P - liczba wszystkih poetów
    - P_active - liczba poetów aktywnych
    - K - pojemność kółka
    - oczekiwalność - wartość czasu mierzona od zakończenia **ODPOCZYNEK**, po przekroczeniu której próbuję organizować koło, wartość losowa, parametryzowana

- zmienne lokalne każdego procesu: 
    - my_ID - ID, nadawane podczas inicjalizacji? zależy od implementacji sprzętowej
    - my_clock - zegar, zegar Lamporta
    - co_przynoszę - [sęp, alkohol, zagrycha],  type(co_przynoszę[i]) = boolean, dla uzgodnionego kółka jest tylko i wyłącznie jedno true w tablicy
    - obrażony_na_koło - len(obrażony_na_koło) = round_up(P/K) gdzie obrażony_na_koło jest TRUE albo FALSE, wartośći zmieniają się losowo
    - co_przynosiłem - [sęp, alkohol, zagrycha], type(co_przynoszę[i]) = liczba całkowite będąca zliczeniem moich aktywnośći w kole
    - jeszcze wiecej potrzeba tutaj dopisać....


---
Wiadomości:

---
Stany:
- Początkowym stanem procesu jest **REST**
- **ODPOCZYNEK** - odpoczywa po libacji, losowa wartość czasu
- **IMPREZZAAAAAA**- libacja trwa
- **REQUESTING** - naszło mnie organizować libację, sprawdzam czy moge
- **WYSYLAM_ZAPRO** - kompletuje {wielkosc kółka -1} osób 
- **KTO_CO_PRZYNOSI** - jestem w kółku, dogaduje kto co ma przynieść
- **OBRAZILEM SIE** - losowo stwierdzilem ze nie chce brac udziału w kółku

---
Szkic algorytmu:

**Opis szczegółowy algorytmu dla procesu i:**


----

### NOTATNIK BRUDNY:
dobra, +- rozpiszę jak to CHYBA MOŻE działać?

proces kiedy jest po odpoczywaniu, i czeka sobie w bezczynności, i od x czasu nikt go nie zaprosił do picia postanawia sam zapraszać 
to się dzieję za pomocą "Ricart-Agrawala dla top-K" kółek i podobno może dziać się w równoległosci, czytaj 2 kółka mogą próbować być formowane na raz -> pożądane, i blokuje to że wszyscy na raz próbują stowrzyć kółko jednoczesnie nie chcąc dołączyć do kółka -> **NIEPOŻĄDANE**

Trzeba pomyśleć że jak zrobi się wstepnie kółko, i okaże się że Px nie chce byc z Py to trzeba to koło rozwiązać i próbować robić od nowa? albo poszukać n nowych chętnych, za tych co się obrazili



dogadywanie się co kto przyniesie to jest inny problem, o którym nawet teraz nie myślę