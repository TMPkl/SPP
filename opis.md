### Stowarzyszenie upitych artystów 

Struktury i zmienne:
- zmienne globalne/ parametry:
    - P - liczba wszystkih poetów
    - P_active - liczba poetów aktywnych
    - K - pojemność kółka
    - oczekiwalność - wartość czasu mierzona od zakończenia **ODPOCZYNEK**, po przekroczeniu której próbuję organizować koło, wartość losowa, parametryzowana

- zmienne lokalne każdego procesu: 
    - my_ID
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