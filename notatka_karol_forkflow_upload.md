### ESP
- esp pobiera .bin po https z `https://ota.kaleszynski.xyz` czy coś w tym stylu
- esp wymaga małej konfiguracji jak zmienia siec ale można zrobić tak że telefonem zmienimy SSID i hasło tak aby nie był to problem
- wiele esp mogą to robić razem na raz
- esp z zawsze może to zrobić jak tylko jest podłączony do wi-fi
- **TODO** 
    - trzeba wymyśleć jaki trigger aby OTA się rozpoczeło
    - jak podłączyć 10 esp na raz do prądu XD  

### SERVER OTA
- prosty serwer który udostępnia folder z 2 plikami, plik .bin oraz version.json (ten drugi jakbym chciał bez triggera tlyko co 30 s sprawdzać czy nie ma nowej wersji)

### Akutualizacja kodu .bin
 
- prosty serwer z jednym endpointem uota.kaleszynski.xyz/update czy coś takiego, zabezpieczenie tokenem tylko, przesyłą plik(i)

## workflow

kod źródłowy -> idf.py build -> plik.bin -> wysłanie PUT do uota.kaleszynski.xyz -> zamiana pliku na serwerze OTA -> trigger ESP -> pobranie i reboot ESP

WYDAJE SIĘ DUŻO ALE TAK NIE JEST
realnie to będzię wymagąło do użytkownika tylko zbudowania i wysłania binarki, reszta zrobi się sama jak się dobrze skonfiguruje (:

### podgląd serial portu 

dodajemy przekierowanie syslogów pi UDP na serwer, łączymy się tailscalem z serwerem i wszystko ładnie widzimy, 

serwer oparty na `Rsyslog` albo `socat`

widać wszystkie wiadomości w jednym pliku -> super bo nie trzeba tego jakoś łączyć czy coś 